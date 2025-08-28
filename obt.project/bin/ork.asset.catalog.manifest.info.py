#!/usr/bin/env ork.python 

from orkengine import core
import argparse
import json
import os
from pathlib import Path
import obt.deco

deco = obt.deco.Deco()

def find_manifest_file(namespace_id):
    """Find the manifest file for a given namespace"""
    manifest_dirs_env = os.environ.get("ORKID_ASSET_MANIFEST_DIRS", "")
    if not manifest_dirs_env:
        return None
    
    manifest_dirs = manifest_dirs_env.split(':')
    for manifest_dir in manifest_dirs:
        if os.path.exists(manifest_dir):
            # Look for manifest files
            for file in Path(manifest_dir).glob("*.json"):
                if file.name != "config.json":
                    # Try to load and check namespace
                    try:
                        manifest = core.AssetManifest.loadFromFile(str(file))
                        if manifest and manifest.namespace == namespace_id:
                            return str(file)
                    except:
                        pass
    return None

def print_manifest_info(catalog, namespace_id):
    """Print detailed information about a manifest"""
    
    manifest = catalog.get_manifest(namespace_id)
    if not manifest:
        print(deco.red(f"No manifest found for namespace: {namespace_id}"))
        return False
    
    print(deco.yellow(f"Manifest for namespace: ") + deco.cyan(namespace_id))
    print(deco.key(f"  Version: ") + deco.val(manifest.version))
    
    # Find the manifest file
    manifest_file = find_manifest_file(namespace_id)
    if manifest_file:
        print(deco.key(f"  File: ") + deco.val(manifest_file))
    
    # Get assets
    assets = manifest.assets
    print(deco.key(f"  Asset count: ") + deco.val(str(len(assets))))
    
    if len(assets) > 0:
        print(deco.yellow("\nAssets:"))
        for asset_id, asset in sorted(assets.items()):
            print(deco.cyan(f"\n  {asset_id}:"))
            print(deco.key(f"    Type: ") + deco.val(asset.type))
            print(deco.key(f"    Priority: ") + deco.val(str(asset.priority)))
            print(deco.key(f"    Platforms: ") + deco.val(', '.join(asset.platforms)))
            
            if hasattr(asset, 'filename') and asset.filename:
                print(deco.key(f"    Filename: ") + deco.val(asset.filename))
            if hasattr(asset, 'storage_hash') and asset.storage_hash:
                print(deco.key(f"    Storage hash: ") + deco.val(asset.storage_hash))
            if hasattr(asset, 'local_loc') and asset.local_loc:
                print(deco.key(f"    Local location: ") + deco.val(asset.local_loc))
            if hasattr(asset, 'remote_loc') and asset.remote_loc:
                print(deco.key(f"    Remote location: ") + deco.val(asset.remote_loc))
            
            # Dependencies
            if hasattr(asset, 'dependencies') and asset.dependencies:
                print(deco.key(f"    Dependencies: ") + deco.val(', '.join(asset.dependencies)))
    
    return True

def print_json_info(catalog, namespace_id):
    """Print manifest information as JSON"""
    
    manifest = catalog.get_manifest(namespace_id)
    if not manifest:
        print(json.dumps({"error": f"No manifest found for namespace: {namespace_id}"}, indent=2))
        return False
    
    info = {
        "namespace": namespace_id,
        "version": manifest.version,
        "asset_count": len(manifest.assets)
    }
    
    # Find the manifest file
    manifest_file = find_manifest_file(namespace_id)
    if manifest_file:
        info["file"] = manifest_file
    
    # Get assets
    info["assets"] = {}
    for asset_id, asset in manifest.assets.items():
        asset_info = {
            "type": asset.type,
            "priority": asset.priority,
            "platforms": asset.platforms
        }
        
        # Add optional fields
        optional_fields = ['filename', 'storage_hash', 'local_loc', 'remote_loc']
        for field in optional_fields:
            if hasattr(asset, field):
                value = getattr(asset, field)
                if value:
                    asset_info[field] = value
        
        if hasattr(asset, 'dependencies') and asset.dependencies:
            asset_info['dependencies'] = list(asset.dependencies)
        
        info["assets"][asset_id] = asset_info
    
    print(json.dumps(info, indent=2))
    return True

def main():
    parser = argparse.ArgumentParser(description='Get detailed information about a manifest')
    parser.add_argument('namespace_id', help='Namespace identifier for the manifest')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    args = parser.parse_args()
    
    core.coreappinit()
    catalog = core.AssetCatalog.instance
    
    # Print manifest info
    if args.json:
        success = print_json_info(catalog, args.namespace_id)
    else:
        success = print_manifest_info(catalog, args.namespace_id)
    
    core.coreappexit()
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())