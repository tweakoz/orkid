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
                # Get asset info for more details
                asset_info = catalog.get_asset_info(asset)
                if asset_info:
                    # size field contains the native_size from manifest (original uncompressed size)
                    size = asset_info.size
                    if size < 1024:
                        size_str = f"{size}B"
                    elif size < 1024 * 1024:
                        size_str = f"{size/1024:.1f}KB"
                    elif size < 1024 * 1024 * 1024:
                        size_str = f"{size/(1024*1024):.1f}MB"
                    else:
                        size_str = f"{size/(1024*1024*1024):.2f}GB"
                    
                    # Get type and other info
                    asset_type = asset_info.type if asset_info.type else "unknown"
                    is_chunked = "chunked" if asset_info.is_chunked() else "single"
                    is_encrypted = "enc" if asset_info.is_encrypted else "plain"
                    
                    # Get hash info (first 8 chars of storage hash)
                    hash_str = ""
                    if asset_info.storage_hash:
                        hash_str = asset_info.storage_hash[:8]
                    
                    # Check if downloaded/unpacked
                    download_info = ""
                    
                    # For asset_pak types, check if unpacked to local_loc
                    if asset_type == "asset_pak":
                        import os
                        from orkengine.core import Path
                        
                        # Get resolved local path where it should be unpacked
                        resolved_path = asset_info.resolved_local_path
                        if resolved_path:
                            # Check if the unpacked location exists
                            local_path_str = resolved_path
                            # For asset_pak, we check if the directory exists and has files
                            if os.path.isdir(local_path_str):
                                # It's unpacked - show location in green
                                local_path = Path(local_path_str)
                                sanitized_loc = local_path.sanitized.toStdString()
                                download_info = "downloaded: " + deco.cyan(sanitized_loc)
                            else:
                                # Check if encrypted file exists in cache
                                encrypted_path = asset_info.local_encrypted_path
                                if encrypted_path:
                                    enc_path_str = encrypted_path
                                    if os.path.isfile(enc_path_str):
                                        # Downloaded but not unpacked - show cached in yellow
                                        download_info = "downloaded: " + deco.yellow("[cached]")
                                    else:
                                        # Not downloaded - show NO in red
                                        download_info = "downloaded: " + deco.red("NO")
                                else:
                                    # Not downloaded - show NO in red
                                    download_info = "downloaded: " + deco.red("NO")
                    
                    # Format the line with aligned columns
                    line = f"    - {asset_id:<20} "
                    line += deco.white(f"[{asset_type:<12}] ")
                    line += deco.cyan(f"{size_str:>8} ")
                    line += deco.yellow(f"{is_chunked:<7} ")
                    line += deco.orange(f"{is_encrypted:<5} ")
                    if hash_str:
                        line += deco.magenta(f"[{hash_str}] ")
                    if download_info:
                        line += download_info
                    print(line)
                else:
                    print(deco.key(f"    - ") + deco.val(asset_id))
        elif len(assets) > 10:
            print(deco.key(f"  Assets: ") + deco.val(f"[{len(assets)} assets - use ork.asset.catalog.list.py to see all]"))
    else:
        print(deco.key(f"  Has manifest: ") + deco.val("No"))
        print(deco.key(f"  Asset count: ") + deco.val("0"))
    
    # Check config
    if config_space:
        # Get namespace remote location from config space
        remote_loc = config_space.getNamespaceRemoteLocation(namespace_id)
        
        if config_space.merged_config:
            merged_config = config_space.merged_config
            
            # Check if namespace has encryption key
            encryption_key = merged_config.getEncryptionKeyForNamespace(namespace_id)
            has_config = bool(encryption_key) or bool(remote_loc)
            
            if has_config:
                print(deco.key(f"  Has config: ") + deco.val("Yes"))
                
                if encryption_key:
                    # Show if encryption key is set (but hide actual value)
                    if encryption_key.startswith("${") and encryption_key.endswith("}"):
                        print(deco.key(f"    Encryption key: ") + deco.orange(f"{encryption_key} (env var)"))
                    else:
                        print(deco.key(f"    Encryption key: ") + deco.val(f"{'*' * 8} (set)"))
                
                if remote_loc:
                    print(deco.key(f"    Remote location: ") + deco.val(remote_loc))
                    
                    # Try to resolve the remote location
                    resolved_location = merged_config.resolveRemoteLocation(remote_loc)
                    if resolved_location:
                        print(deco.key(f"    Resolved locations:"))
                        print(deco.key(f"      Download URL: ") + deco.cyan(resolved_location.download_url))
                        print(deco.key(f"      Upload URL: ") + deco.cyan(resolved_location.upload_url))
                        
                        # Show API keys if configured
                        if hasattr(resolved_location, 'api_key_read') and resolved_location.api_key_read:
                            api_key = resolved_location.api_key_read
                            if api_key == "<PasswordAuthentication>":
                                print(deco.key(f"      Download auth: ") + deco.orange("Password authentication required"))
                            elif api_key.startswith("${") and api_key.endswith("}"):
                                print(deco.key(f"      Download auth: ") + deco.orange(f"{api_key} (env var)"))
                            else:
                                print(deco.key(f"      Download auth: ") + deco.val("API key configured"))
                        
                        if hasattr(resolved_location, 'api_key_write') and resolved_location.api_key_write:
                            api_key = resolved_location.api_key_write
                            if api_key == "<PasswordAuthentication>":
                                print(deco.key(f"      Upload auth: ") + deco.orange("Password authentication required"))
                            elif api_key.startswith("${") and api_key.endswith("}"):
                                print(deco.key(f"      Upload auth: ") + deco.orange(f"{api_key} (env var)"))
                            else:
                                print(deco.key(f"      Upload auth: ") + deco.val("API key configured"))
                        
                        if hasattr(resolved_location, 'disable_cert_check') and resolved_location.disable_cert_check:
                            print(deco.key(f"      TLS verification: ") + deco.orange("Disabled"))
            else:
                print(deco.key(f"  Has config: ") + deco.val("No"))
        else:
            print(deco.key(f"  Has config: ") + deco.val("No"))
    
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
            remote_location = merged_config.getRemoteLocationForNamespace(namespace_id)
            if remote_location:
                info["config"]["remote_location"] = remote_location
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
    catalog = core.AssetCatalog.instance
    cfgspc = catalog.config_space
        
    # Print namespace info
    if args.json:
        success = print_json_info(catalog, args.namespace_id, cfgspc)
    else:
        success = print_namespace_info(catalog, args.namespace_id, cfgspc)
    
    core.coreappexit()
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())