#!/usr/bin/env python3

################################################################################
# Orkid Asset Manifest Generator
# Generates encrypted assets and manifest files
################################################################################

import argparse
import json
import os
import hashlib
import tarfile
import tempfile
import subprocess
from pathlib import Path
from datetime import datetime
from obt import crypt, path as obt_path
from orkengine import core

class AssetGenerator:
    def __init__(self, cache_dir=None, override_key=None):
        self.cache_dir = Path(cache_dir) if cache_dir else obt_path.temp() / "orkid_asset_builds"
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.override_key = override_key
        
    def calculate_md5(self, filepath):
        """Calculate MD5 hash of a file"""
        hash_md5 = hashlib.md5()
        with open(filepath, "rb") as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_md5.update(chunk)
        return hash_md5.hexdigest()
    
    def create_asset_pak(self, source_dir, namespace, asset_id, strip_leading=False):
        """Create tar file from directory, encrypt it, and cache it"""
        source_dir = Path(source_dir).resolve()
        temp_tar = tempfile.NamedTemporaryFile(suffix='.tar', delete=False)
        
        # Create tar archive
        with tarfile.open(temp_tar.name, 'w') as tar:
            for root, dirs, files in os.walk(source_dir):
                for file in files:
                    full_path = Path(root) / file
                    if strip_leading:
                        # Strip the source_dir from the path, keeping only the relative part
                        rel_path = full_path.relative_to(source_dir)
                        arcname = str(rel_path)
                    else:
                        # Keep the full path structure including source_dir name
                        arcname = str(full_path)
                    tar.add(str(full_path), arcname=arcname)
        
        # Get encryption key for namespace
        enc_key = self.get_encryption_key(namespace, self.override_key)
        print(f"Using encryption key<{enc_key}> for namespace: {namespace}" )
        
        # Encrypt the tar file to a temporary location first
        # Use mktemp to get a unique name that doesn't exist
        temp_enc_path = tempfile.mktemp(suffix='.enc', dir=str(self.cache_dir))
        
        crypt.encrypt_file(temp_tar.name, temp_enc_path, enc_key)
        
        # Calculate MD5 of the encrypted file
        md5_hash = self.calculate_md5(temp_enc_path)
        
        # Move encrypted file to cache with MD5-based name
        encrypted_file = self.cache_dir / f"{md5_hash}.enc"
        print(f"Encrypted asset pak path: {encrypted_file}")
        if encrypted_file.exists():
            os.unlink(encrypted_file)
        os.rename(temp_enc_path, str(encrypted_file))
        
        # Clean up temp tar
        os.unlink(temp_tar.name)
        
        # Generate receipt for external use
        receipt = {
            "generated_at": datetime.now().isoformat(),
            "namespace": namespace,
            "asset_id": asset_id,
            "type": "asset_pak",
            "source_dir": str(source_dir),
            "cached_file": str(encrypted_file),
            "file_size": encrypted_file.stat().st_size,
            "md5": md5_hash
        }
        
        receipt_file = self.cache_dir / f"{md5_hash}.receipt.json"
        with open(receipt_file, 'w') as f:
            json.dump(receipt, f, indent=2)
        
        return encrypted_file, md5_hash, receipt_file
    
    def create_asset_file(self, source_file, namespace, asset_id):
        """Encrypt single file and cache it"""
        source_file = Path(source_file).resolve()
        
        # Get encryption key
        enc_key = self.get_encryption_key(namespace, self.override_key)
        
        # Encrypt the file to a temporary location first
        # Use mktemp to get a unique name that doesn't exist
        temp_enc_path = tempfile.mktemp(suffix='.enc', dir=str(self.cache_dir))
        
        crypt.encrypt_file(str(source_file), temp_enc_path, enc_key)
        
        # Calculate MD5 of the encrypted file
        md5_hash = self.calculate_md5(temp_enc_path)
        
        # Move encrypted file to cache with MD5-based name
        cached_file = self.cache_dir / f"{md5_hash}.enc"
        if cached_file.exists():
            os.unlink(cached_file)
        os.rename(temp_enc_path, str(cached_file))
        
        # Generate receipt for external use
        receipt = {
            "generated_at": datetime.now().isoformat(),
            "namespace": namespace,
            "asset_id": asset_id,
            "type": "asset",
            "source_file": str(source_file),
            "cached_file": str(cached_file),
            "file_size": cached_file.stat().st_size,
            "md5": md5_hash
        }
        
        receipt_file = self.cache_dir / f"{md5_hash}.receipt.json"
        with open(receipt_file, 'w') as f:
            json.dump(receipt, f, indent=2)
        
        return cached_file, md5_hash, receipt_file
    
    def get_encryption_key(self, namespace, override_key=None):
        """Get encryption key for namespace"""
        # First check if override key provided
        if override_key:
            return override_key
            
        # Then check environment variable
        env_key = f"ORKID_KEY_{namespace.upper()}"
        key = os.environ.get(env_key)
        if not key:
            # Fallback to known keys (temporary)
            known_keys = {
                "singularity": "singularity_rulez"
            }
            key = known_keys.get(namespace)
            if not key:
                raise ValueError(f"No encryption key found for namespace '{namespace}'. Set {env_key} environment variable or use --key option.")
        return key

def main():
    parser = argparse.ArgumentParser(description='Generate Orkid assets and manifests')
    
    # Manifest args
    parser.add_argument('--namespace', required=True, help='Asset namespace')
    parser.add_argument('--output', '-o', required=True, help='Output manifest file')
    parser.add_argument('--append', action='store_true', help='Append to existing manifest')
    
    # Asset args
    parser.add_argument('--asset-id', required=True, help='Asset identifier')
    parser.add_argument('--priority', type=int, default=100, help='Priority (lower wins)')
    parser.add_argument('--src-loc', required=True, help='CDN location template (e.g., <orkid_cdn>)')
    parser.add_argument('--dst-loc', required=True, help='Destination location')
    
    # Asset type + source (mutually exclusive)
    asset_group = parser.add_mutually_exclusive_group(required=True)
    asset_group.add_argument('--asset-pak', metavar='DIR', help='Create asset_pak from directory')
    asset_group.add_argument('--asset', metavar='FILE', help='Create asset from file')
    
    # Optional args
    parser.add_argument('--cache-dir', help='Cache directory for generated assets')
    parser.add_argument('--merge', type=lambda x: x.lower() == 'true', help='For asset_pak: merge with existing')
    parser.add_argument('--dependencies', nargs='*', help='Dependencies (namespace.asset_id)')
    parser.add_argument('--key', help='Encryption key (alternative to environment variable)')
    parser.add_argument('--filename', help='Override filename in manifest (without path)')
    parser.add_argument('--strip-leading', action='store_true', help='Strip base directory from tar archive paths')
    
    # SCP upload args
    parser.add_argument('--scp-upload', action='store_true', help='Upload to SCP destination from config')
    
    args = parser.parse_args()
    
    # Determine type and source
    if args.asset_pak:
        asset_type = 'asset_pak'
        source = Path(args.asset_pak)
        if not source.is_dir():
            parser.error(f"asset_pak source must be a directory: {source}")
    else:
        asset_type = 'asset'
        source = Path(args.asset)
        if not source.is_file():
            parser.error(f"asset source must be a file: {source}")
    
    # Initialize generator
    generator = AssetGenerator(args.cache_dir, args.key)
    
    # Generate the asset
    try:
        if asset_type == 'asset_pak':
            cached_file, md5_hash, receipt = generator.create_asset_pak(
                source, args.namespace, args.asset_id, args.strip_leading
            )
            if args.filename:
                filename = args.filename
            else:
                filename = f"{args.namespace}_{args.asset_id}.tar.enc"
        else:
            cached_file, md5_hash, receipt = generator.create_asset_file(
                source, args.namespace, args.asset_id
            )
            if args.filename:
                filename = args.filename
            else:
                # Keep original extension for single files
                orig_ext = source.suffix
                filename = f"{args.namespace}_{args.asset_id}{orig_ext}.enc"
    except Exception as e:
        print(f"Error generating asset: {e}")
        return 1
    
    # Create manifest entry
    entry = {
        "type": asset_type,
        "priority": args.priority,
        "src_loc": args.src_loc,
        "dst_loc": args.dst_loc,
        "filename": filename,
        "md5": md5_hash
    }
    
    if asset_type == "asset_pak" and args.merge is not None:
        entry["merge"] = args.merge
    
    # Always include dependencies (empty dict if none specified)
    entry["dependencies"] = {}
    if args.dependencies:
        for dep in args.dependencies:
            if '.' in dep:
                dep_ns, dep_id = dep.split('.', 1)
                entry["dependencies"][dep] = {}
            else:
                print(f"Warning: dependency should be in format namespace.asset_id, got: {dep}")
    
    # Load or create manifest
    manifest = {}
    if args.append and os.path.exists(args.output):
        with open(args.output, 'r') as f:
            manifest = json.load(f)
            # Validate it's a new-format manifest
            if "namespace" not in manifest:
                print(f"Error: Existing manifest is not in new format. Please update it first.")
                return 1
            if manifest["namespace"] != args.namespace:
                print(f"Error: Cannot append to manifest with different namespace ({manifest['namespace']} != {args.namespace})")
                return 1
    else:
        manifest = {
            "namespace": args.namespace,
            "version": "1.0.0",
            "assets": {}
        }
    
    # Add asset
    manifest["assets"][args.asset_id] = entry
    
    # Write manifest
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    
    print(f"✓ Generated asset: {cached_file}")
    print(f"✓ MD5: {md5_hash}")
    print(f"✓ Receipt saved: {receipt}")
    print(f"✓ Manifest updated: {args.output}")
    
    # Handle SCP upload if requested
    if args.scp_upload:
        # Initialize core and load config to get scp_destination
        core.coreappinit()
        
        # Create fetcher to load configs
        fetcher = core.AssetFetcher()
        fetcher.reload()
        config = fetcher.get_config()
        
        # Debug: print manifest directories
        import os
        manifest_dirs = os.environ.get('ORKID_ASSET_MANIFEST_DIRS', '').split(':')
        print(f"Manifest directories: {manifest_dirs}")
        
        # Debug: print all locations
        print(f"Available locations: {list(config.locations.keys())}")
        for loc_name, loc_info in config.locations.items():
            print(f"  {loc_name}: {loc_info}")
        
        # Extract location name from src_loc (e.g., "<devcdn>" -> "devcdn")
        src_loc = args.src_loc
        location_name = None
        if src_loc.startswith('<') and src_loc.endswith('>'):
            location_name = src_loc[1:-1]
        
        if not location_name:
            print(f"✗ Cannot determine location name from src_loc: {src_loc}")
            return 1
            
        # Get the LocationInfo for this location
        if location_name not in config.locations:
            print(f"✗ Location '{location_name}' not found in config")
            print(f"Available locations: {list(config.locations.keys())}")
            return 1
            
        location_info = config.locations[location_name]
        if not location_info:
            print(f"✗ LocationInfo is None for '{location_name}'")
            return 1
            
        # Debug: print location info
        print(f"LocationInfo for '{location_name}': {location_info}")
        
        if not hasattr(location_info, 'scp_destination') or not location_info.scp_destination:
            print(f"✗ No scp_destination configured for location '{location_name}'")
            return 1
            
        scp_dest = location_info.scp_destination
        print(f"\nUploading asset to {scp_dest}...")
        
        try:
            # Ensure destination ends with /
            if not scp_dest.endswith('/'):
                scp_dest += '/'
            
            # Construct full destination path with filename
            full_dest = scp_dest + filename
            
            # Run SCP in foreground so user can provide auth info
            scp_cmd = ['scp', str(cached_file), full_dest]
            print(f"Running: {' '.join(scp_cmd)}")
            result = subprocess.run(scp_cmd)
            
            if result.returncode == 0:
                print(f"✓ Uploaded to: {full_dest}")
            else:
                print(f"✗ SCP upload failed with exit code: {result.returncode}")
                return 1
        except Exception as e:
            print(f"✗ SCP upload error: {e}")
            return 1
    
    return 0

if __name__ == "__main__":
    exit(main())