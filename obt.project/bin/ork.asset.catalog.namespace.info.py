#!/usr/bin/env ork.python 

from orkengine import core
import argparse
import json
import obt.deco

deco = obt.deco.Deco()

def print_namespace_info(catalog, namespace_id, config_space):
    """Print detailed information about a namespace"""
    
    # Get namespace object
    ns = catalog.find_namespace(namespace_id)
    if not ns:
        print(deco.red(f"Namespace not found: {namespace_id}"))
        return False
    
    print(deco.white(f"Namespace: ") + deco.val(namespace_id))
    
    # Get manifest for this namespace
    manifest = catalog.get_manifest(namespace_id)
    if manifest:
        print(deco.key(f"  Has manifest: ") + deco.val("Yes"))
        print(deco.key(f"  Manifest version: ") + deco.val(manifest.version))
        
        # Count assets
        assets = catalog.list_assets(f"{namespace_id}|*")
        print(deco.key(f"  Asset count: ") + deco.val(str(len(assets))))
        
        # List assets if not too many
        if len(assets) > 0 and len(assets) <= 10:
            print(deco.key(f"  Assets:"))
            for asset in sorted(assets):
                asset_id = asset.split('|')[-1]
                print(deco.key(f"    - ") + deco.val(asset_id))
        elif len(assets) > 10:
            print(deco.key(f"  Assets: ") + deco.val(f"[{len(assets)} assets - use ork.asset.catalog.list.py to see all]"))
    else:
        print(deco.key(f"  Has manifest: ") + deco.val("No"))
        print(deco.key(f"  Asset count: ") + deco.val("0"))
    
    # Check config
    if config_space and config_space.merged_config:
        merged_config = config_space.merged_config
        
        # Check if namespace has encryption key
        encryption_key = merged_config.getEncryptionKeyForNamespace(namespace_id)
        if encryption_key:
            print(f"  Has config: Yes")
            print(f"    Encryption key: {'*' * 8} (hidden)")
            upload_location = merged_config.getUploadLocationForNamespace(namespace_id)
            if upload_location:
                print(f"    Upload location: {upload_location}")
        else:
            print(f"  Has config: No")
    
    return True

def print_json_info(catalog, namespace_id, config_space):
    """Print namespace information as JSON"""
    
    ns = catalog.find_namespace(namespace_id)
    if not ns:
        print(json.dumps({"error": f"Namespace not found: {namespace_id}"}, indent=2))
        return False
    
    info = {
        "namespace": namespace_id,
        "exists": True
    }
    
    # Get manifest info
    manifest = catalog.get_manifest(namespace_id)
    if manifest:
        info["manifest"] = {
            "exists": True,
            "version": manifest.version
        }
        
        # Count assets
        assets = catalog.list_assets(f"{namespace_id}|*")
        info["asset_count"] = len(assets)
        
        if len(assets) <= 10:
            info["assets"] = [asset.split('|')[-1] for asset in sorted(assets)]
    else:
        info["manifest"] = {"exists": False}
        info["asset_count"] = 0
    
    # Check config
    if config_space and config_space.merged_config:
        merged_config = config_space.merged_config
        
        encryption_key = merged_config.getEncryptionKeyForNamespace(namespace_id)
        if encryption_key:
            info["config"] = {
                "exists": True,
                "has_encryption_key": True
            }
            upload_location = merged_config.getUploadLocationForNamespace(namespace_id)
            if upload_location:
                info["config"]["upload_location"] = upload_location
        else:
            info["config"] = {"exists": False}
    
    print(json.dumps(info, indent=2))
    return True

def main():
    parser = argparse.ArgumentParser(description='Get detailed information about a namespace')
    parser.add_argument('namespace_id', help='Namespace identifier')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    args = parser.parse_args()
    
    core.coreappinit()
    
    # Create config space and catalog
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    catalog = core.AssetCatalog(space=cfgspc)
    
    # Load from global manifests
    core.AssetCatalog.loadFromGlobalManifests(catalog)
    
    # Print namespace info
    if args.json:
        success = print_json_info(catalog, args.namespace_id, cfgspc)
    else:
        success = print_namespace_info(catalog, args.namespace_id, cfgspc)
    
    core.coreappexit()
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())