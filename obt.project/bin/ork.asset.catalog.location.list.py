#!/usr/bin/env ork.python 

from orkengine import core
import os
from pathlib import Path
import obt.deco

deco = obt.deco.Deco()

core.coreappinit()

# Create config space and catalog
cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
catalog = core.AssetCatalog(space=cfgspc)

# Load from global manifests
core.AssetCatalog.loadFromGlobalManifests(catalog)

print(deco.yellow("Configured Locations:") + "\n")

# Get manifest directories
manifest_dirs_env = os.environ.get("ORKID_ASSET_MANIFEST_DIRS", "")
if not manifest_dirs_env:
    print(deco.red("ORKID_ASSET_MANIFEST_DIRS not set"))
    core.coreappexit()
    exit(1)

manifest_dirs = manifest_dirs_env.split(':')

# Collect all locations and destinations
all_locations = {}
all_destinations = {}
config_sources = {}

for manifest_dir in manifest_dirs:
    config_file = Path(manifest_dir) / "config.json"
    if config_file.exists():
        try:
            import json
            with open(config_file, 'r') as f:
                config_data = json.load(f)
            
            # Collect locations
            if 'locations' in config_data:
                for loc_id, loc_value in config_data['locations'].items():
                    if loc_id not in all_locations:
                        all_locations[loc_id] = loc_value
                        config_sources[loc_id] = str(config_file)
            
            # Collect destinations
            if 'destinations' in config_data:
                for dest_id, dest_value in config_data['destinations'].items():
                    if dest_id not in all_destinations:
                        all_destinations[dest_id] = dest_value
                        config_sources[f"dest_{dest_id}"] = str(config_file)
                        
        except Exception as e:
            print(deco.red(f"Error reading {config_file}: {e}"))

# Print remote locations
if all_locations:
    print(deco.magenta("Remote Locations:"))
    for loc_id in sorted(all_locations.keys()):
        loc_value = all_locations[loc_id]
        print(f"  {deco.cyan(loc_id)}:")
        if isinstance(loc_value, dict):
            # Print complex location details
            if 'url' in loc_value:
                print(f"    {deco.key('URL:')} {deco.val(loc_value['url'])}")
            if 'scp_destination' in loc_value:
                print(f"    {deco.key('SCP Destination:')} {deco.val(loc_value['scp_destination'])}")
            if 'api_key' in loc_value:
                print(f"    {deco.key('API Key:')} {deco.red('*' * 8 + ' (hidden)')}")
            if 'disable_cert_check' in loc_value:
                print(f"    {deco.key('Certificate Check:')} {deco.val('Disabled' if loc_value['disable_cert_check'] else 'Enabled')}")
            # Print any other fields
            for k, v in loc_value.items():
                if k not in ['url', 'scp_destination', 'api_key', 'disable_cert_check']:
                    print(f"    {deco.key(k + ':')} {deco.val(str(v))}")
        else:
            print(f"    {deco.key('URL:')} {deco.val(loc_value)}")
        
        # Determine location type
        if isinstance(loc_value, dict):
            # Complex location with multiple fields
            if 'url' in loc_value:
                url = loc_value['url']
                if url.startswith("https://"):
                    loc_type = "HTTPS (Complex)"
                elif url.startswith("http://"):
                    loc_type = "HTTP (Complex)"
                else:
                    loc_type = "Complex"
            else:
                loc_type = "Complex Configuration"
        elif isinstance(loc_value, str):
            if loc_value.startswith("https://"):
                loc_type = "HTTPS"
            elif loc_value.startswith("http://"):
                loc_type = "HTTP"
            elif loc_value.startswith("scp://"):
                loc_type = "SCP/SSH"
            elif loc_value.startswith("s3://"):
                loc_type = "AWS S3"
            elif loc_value.startswith("file://"):
                loc_type = "Local File"
            else:
                loc_type = "Unknown"
        else:
            loc_type = "Unknown"
        
        print(f"    {deco.key('Type:')} {deco.val(loc_type)}")
        print(f"    {deco.key('Source:')} {deco.val(config_sources[loc_id])}")
        
        # Check which namespaces use this location
        using_namespaces = []
        for manifest_dir in manifest_dirs:
            config_file = Path(manifest_dir) / "config.json"
            if config_file.exists():
                try:
                    with open(config_file, 'r') as f:
                        config_data = json.load(f)
                    if 'namespaces' in config_data:
                        for ns_id, ns_config in config_data['namespaces'].items():
                            if 'remote_location' in ns_config and ns_config['remote_location'] == loc_id:
                                using_namespaces.append(ns_id)
                except:
                    pass
        
        if using_namespaces:
            print(f"    {deco.key('Used by namespaces:')} {deco.val(', '.join(sorted(set(using_namespaces))))}")
        print()

# Print local destinations
if all_destinations:
    print(deco.magenta("Local Destinations:"))
    for dest_id in sorted(all_destinations.keys()):
        dest_value = all_destinations[dest_id]
        print(f"  {deco.cyan(f'<{dest_id}>')}:")
        print(f"    {deco.key('Template:')} {deco.val(dest_value)}")
        
        # Try to resolve the path
        resolved_path = dest_value
        if "<stage>" in resolved_path:
            stage_dir = os.environ.get("OBT_STAGE", "~/.staging")
            resolved_path = resolved_path.replace("<stage>", stage_dir)
        if "<temp>" in resolved_path:
            temp_dir = "/tmp"
            resolved_path = resolved_path.replace("<temp>", temp_dir)
        if "<assetcache>" in resolved_path:
            cache_dir = os.path.join(os.environ.get("OBT_STAGE", "~/.staging"), "assetcache")
            resolved_path = resolved_path.replace("<assetcache>", cache_dir)
        
        resolved_path = os.path.expanduser(resolved_path)
        print(f"    {deco.key('Resolves to:')} {deco.val(resolved_path)}")
        
        # Check if path exists
        if os.path.exists(resolved_path):
            print(f"    {deco.key('Exists:')} {deco.val('Yes')}")
            if os.path.isdir(resolved_path):
                try:
                    num_files = len(os.listdir(resolved_path))
                    print(f"    {deco.key('Contents:')} {deco.val(f'{num_files} items')}")
                except:
                    pass
        else:
            print(f"    {deco.key('Exists:')} {deco.red('No')}")
        
        print(f"    {deco.key('Source:')} {deco.val(config_sources[f'dest_{dest_id}'])}")
        print()

# Summary
total_locations = len(all_locations)
total_destinations = len(all_destinations)
print(f"{deco.key('Total:')} {deco.val(str(total_locations))} remote locations, {deco.val(str(total_destinations))} local destinations")

core.coreappexit()