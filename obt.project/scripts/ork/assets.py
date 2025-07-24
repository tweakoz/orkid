#!/usr/bin/env python3
################################################################################
# Orkid Asset Management Module
################################################################################

import os
import json
import glob
from obt import path
from pathlib import Path

def load_all_manifests():
  """Load all manifests from ORKID_ASSET_MANIFEST_DIRS with priority resolution"""
  resolved_assets = {}
  
  # Get manifest directories from environment
  manifest_dirs_env = os.environ.get('ORKID_ASSET_MANIFEST_DIRS', '')
  if not manifest_dirs_env:
    # Default to the new location
    manifest_dirs = [str(path.orkid()/"ork.data"/"asset_manifests")]
  else:
    manifest_dirs = manifest_dirs_env.split(':')
  
  # Process each directory
  for manifest_dir in manifest_dirs:
    if not os.path.exists(manifest_dir):
      continue
      
    # Find all JSON files in the directory
    for manifest_file in glob.glob(os.path.join(manifest_dir, "*.json")):
      try:
        with open(manifest_file, 'r') as f:
          manifest = json.load(f)
          
        # Handle both old and new manifest formats
        if "namespace" in manifest:
          # New format
          namespace = manifest["namespace"]
          for asset_id, asset_data in manifest.get("assets", {}).items():
            full_id = f"{namespace}.{asset_id}"
            asset_data["namespace"] = namespace
            asset_data["manifest_source"] = manifest_file
            
            # Priority resolution
            if full_id in resolved_assets:
              existing_priority = resolved_assets[full_id].get("priority", 100)
              new_priority = asset_data.get("priority", 100)
              if new_priority < existing_priority:
                resolved_assets[full_id] = asset_data
            else:
              resolved_assets[full_id] = asset_data
        else:
          # Old format - convert to new format
          for asset_id, asset_data in manifest.items():
            # Assume singularity namespace for old format
            namespace = "singularity"
            full_id = f"{namespace}.{asset_id}"
            
            # Convert old field names
            if "dest_path" in asset_data:
              asset_data["dst_loc"] = asset_data.pop("dest_path")
            if "loc" in asset_data:
              asset_data["src_loc"] = asset_data.pop("loc")
            
            asset_data["namespace"] = namespace
            asset_data["manifest_source"] = manifest_file
            asset_data["priority"] = 100  # Default priority
            
            resolved_assets[full_id] = asset_data
            
      except Exception as e:
        print(f"Error loading manifest {manifest_file}: {e}")
        continue
  
  return resolved_assets

def load_all_configs():
  """Load and merge all configuration files from manifest directories"""
  from obt import path as obt_path
  import yarl
  
  merged_config = {
    "namespace_keys": {},
    "locations": {},
    "destinations": {}
  }
  
  # Get manifest directories from environment
  manifest_dirs_env = os.environ.get('ORKID_ASSET_MANIFEST_DIRS', '')
  if not manifest_dirs_env:
    # Default to the new location
    manifest_dirs = [str(path.orkid()/"ork.data"/"asset_manifests")]
  else:
    manifest_dirs = manifest_dirs_env.split(':')
  
  # Process each directory
  for manifest_dir in manifest_dirs:
    if not os.path.exists(manifest_dir):
      continue
      
    # Find all config files in the directory (*config.json matches both config.json and *_config.json)
    for config_file in glob.glob(os.path.join(manifest_dir, "*config.json")):
      try:
        with open(config_file, 'r') as f:
          config = json.load(f)
          
        # Merge each section (later directories override earlier ones)
        if "namespace_keys" in config:
          merged_config["namespace_keys"].update(config["namespace_keys"])
        if "locations" in config:
          # Convert string URLs to yarl.URL objects
          for key, value in config["locations"].items():
            if value.startswith("http"):
              merged_config["locations"][key] = yarl.URL(value)
            else:
              merged_config["locations"][key] = value
        if "destinations" in config:
          merged_config["destinations"].update(config["destinations"])
          
      except Exception as e:
        print(f"Error loading config {config_file}: {e}")
        continue
  
  # Process destination templates
  processed_dests = {}
  for key, value in merged_config["destinations"].items():
    if value.startswith("<stage>"):
      processed_dests[key] = obt_path.stage() / value.replace("<stage>/", "").replace("<stage>", "")
    elif value.startswith("<temp>"):
      processed_dests[key] = obt_path.temp() / value.replace("<temp>/", "").replace("<temp>", "")
    else:
      processed_dests[key] = Path(value)
  
  merged_config["destinations"] = processed_dests
  
  return merged_config

def fetch_pak(pack_identifier):
  """Fetch asset pack(s) by identifier
  
  Args:
    pack_identifier: Can be either:
      - Full asset ID: "namespace.asset_id" 
      - Namespace only: "namespace" (fetches all assets in namespace)
  
  Returns:
    Number of assets fetched
  """
  # Load configuration
  config = load_all_configs()
  namespace_keys = config["namespace_keys"]
  known_locs = config["locations"]
  known_dests = config["destinations"]
  
  # Load all manifests
  all_assets = load_all_manifests()
  
  # Track how many assets were fetched
  fetch_count = 0
  
  # Handle pack identifier
  if '.' in pack_identifier:
    # Full format: namespace.asset_id
    if pack_identifier in all_assets:
      asset_data = all_assets[pack_identifier]
      fetch(pack_identifier, asset_data, known_locs, known_dests, namespace_keys)
      fetch_count = 1
    else:
      print(f"Asset {pack_identifier} not found in manifests")
      return 0
  else:
    # Namespace only - fetch all assets in that namespace
    for asset_id, asset_data in all_assets.items():
      namespace, aid = asset_id.split('.', 1)
      if pack_identifier == namespace:
        fetch(asset_id, asset_data, known_locs, known_dests, namespace_keys)
        fetch_count += 1
    
    if fetch_count == 0:
      print(f"No assets found matching '{pack_identifier}'")
      print("Note: You must specify the full asset ID as namespace.asset_id (e.g., singularity.std)")
      print("Or you can specify just a namespace to fetch all assets in that namespace")
  
  return fetch_count

def fetch(asset_id, asset_data, known_locs, known_dests, namespace_keys):
  """Fetch and process a single asset"""
  from obt import wget, crypt, command
  import yarl
  
  asset_type = asset_data.get("type")
  namespace = asset_data.get("namespace", "default")
  
  if asset_type == "asset_pak":
    dst_loc = asset_data["dst_loc"]
    if dst_loc.startswith("<"): 
      # dst_root is name between < and >
      l_angle = dst_loc.find("<")
      r_angle = dst_loc.find(">")
      dst_root = dst_loc[l_angle+1:r_angle]
      if dst_root in known_dests:
        dst_loc = dst_loc.replace(f"<{dst_root}>",str(known_dests[dst_root]))
      
    src_loc = asset_data["src_loc"]
    filename = asset_data["filename"]
    if src_loc.startswith("<"):
      l_angle = src_loc.find("<")
      r_angle = src_loc.find(">")
      src_root = src_loc[l_angle+1:r_angle]
      if src_root in known_locs:
        src_loc = src_loc.replace(f"<{src_root}>",str(known_locs[src_root]))
      src_loc += "/" + filename
      
    print(f"fetching asset_pak {asset_id}")
    fetched = wget.wget(urls=[src_loc],output_name=filename,md5val=asset_data["md5"])
    
    if fetched != None:
      if namespace in namespace_keys:
        dec_key = namespace_keys[namespace]
        decoded = str(fetched)+".dec"
        print(f"decrypting asset_pak {asset_id}")
        crypt.decrypt_file(fetched,decoded,dec_key)
        print(f"extracting asset_pak {asset_id}")
        cmd_list = [
          "tar",
          "xf",
          decoded,
          "-C",
          dst_loc
        ]
        command.capture(cmd_list)
        
  elif asset_type == "asset":
    # Single file asset
    dst_loc = asset_data["dst_loc"]
    src_loc = asset_data["src_loc"]
    filename = asset_data["filename"]
    
    # Process src_loc
    if src_loc.startswith("<"):
      l_angle = src_loc.find("<")
      r_angle = src_loc.find(">")
      src_root = src_loc[l_angle+1:r_angle]
      if src_root in known_locs:
        src_loc = src_loc.replace(f"<{src_root}>",str(known_locs[src_root]))
      src_loc += "/" + filename
    
    # Process dst_loc
    if dst_loc.startswith("<"):
      # Replace symbolic paths in dst_loc
      for sym, real_path in known_dests.items():
        dst_loc = dst_loc.replace(f"<{sym}>", str(real_path))
    
    print(f"fetching asset {asset_id}")
    fetched = wget.wget(urls=[src_loc],output_name=filename,md5val=asset_data["md5"])
    
    if fetched != None:
      if namespace in namespace_keys:
        dec_key = namespace_keys[namespace]
        print(f"decrypting asset {asset_id}")
        # Ensure destination directory exists
        dst_path = Path(dst_loc)
        dst_path.parent.mkdir(parents=True, exist_ok=True)
        # Decrypt directly to destination
        crypt.decrypt_file(fetched, str(dst_path), dec_key)