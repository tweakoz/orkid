#!/usr/bin/env ork.python 

from orkengine import core
import argparse
import json
import os
from pathlib import Path
import obt.deco

deco = obt.deco.Deco()

def find_config_file_with_namespace(namespace_id):
    """Find which config file contains a namespace configuration"""
    manifest_dirs_env = os.environ.get("ORKID_ASSET_MANIFEST_DIRS", "")
    if not manifest_dirs_env:
        return None
    
    manifest_dirs = manifest_dirs_env.split(':')
    for manifest_dir in manifest_dirs:
        config_file = Path(manifest_dir) / "config.json"
        if config_file.exists():
            try:
                config = core.AssetConfig.loadFromFile(str(config_file))
                if config and hasattr(config, 'namespaces'):
                    if namespace_id in config.namespaces:
                        return str(config_file)
            except:
                pass
    return None

def print_config_info(catalog, namespace_id, config_space):
    """Print configuration information for a namespace"""
    
    if not config_space or not config_space.merged_config:
        print(deco.red("No configuration loaded"))
        return False
    
    merged_config = config_space.merged_config
    
    encryption_key = merged_config.getEncryptionKeyForNamespace(namespace_id)
    if not encryption_key:
        print(deco.red(f"No configuration found for namespace: {namespace_id}"))
        return False
    
    print(f"{deco.yellow('Configuration for namespace:')} {deco.cyan(namespace_id)}")
    
    # Find which file contains this config
    config_file = find_config_file_with_namespace(namespace_id)
    if config_file:
        print(f"  {deco.key('Source file:')} {deco.val(config_file)}")
    
    print(f"  {deco.key('Encryption key:')} {deco.red('*' * 8 + ' (hidden)')}")
    
    upload_location = merged_config.getRemoteLocationForNamespace(namespace_id)
    if upload_location:
        print(f"  {deco.key('Upload location:')} {deco.val(upload_location)}")
    
    # Show destinations that might be used
    if hasattr(merged_config, 'destinations') and merged_config.destinations:
        print(deco.yellow("\nAvailable destination variables:"))
        for dest_name, dest_path in sorted(merged_config.destinations.items()):
            print(f"  {deco.cyan(f'<{dest_name}>:')} {deco.val(dest_path)}")
    
    return True

def print_json_info(catalog, namespace_id, config_space):
    """Print configuration information as JSON"""
    
    if not config_space or not config_space.merged_config:
        print(json.dumps({"error": "No configuration loaded"}, indent=2))
        return False
    
    merged_config = config_space.merged_config
    
    encryption_key = merged_config.getEncryptionKeyForNamespace(namespace_id)
    if not encryption_key:
        print(json.dumps({"error": f"No configuration found for namespace: {namespace_id}"}, indent=2))
        return False
    
    info = {
        "namespace": namespace_id,
        "has_encryption_key": True
    }
    
    # Find which file contains this config
    config_file = find_config_file_with_namespace(namespace_id)
    if config_file:
        info["source_file"] = config_file
    
    upload_location = merged_config.getRemoteLocationForNamespace(namespace_id)
    if upload_location:
        info["upload_location"] = upload_location
    
    # Include destinations
    if hasattr(merged_config, 'destinations') and merged_config.destinations:
        info["destinations"] = merged_config.destinations
    
    print(json.dumps(info, indent=2))
    return True

def main():
    parser = argparse.ArgumentParser(description='Get configuration information for a namespace')
    parser.add_argument('namespace_id', help='Namespace identifier')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    args = parser.parse_args()
    
    core.coreappinit()
    
    catalog = core.AssetCatalog.instance
    cfgspc = catalog.config_space

    # Print config info
    if args.json:
        success = print_json_info(catalog, args.namespace_id, cfgspc)
    else:
        success = print_config_info(catalog, args.namespace_id, cfgspc)
    
    core.coreappexit()
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())