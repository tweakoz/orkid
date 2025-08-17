#!/usr/bin/env ork.python 

import argparse
import json
from orkengine import core
from ork import path as ork_path
from ork import assets as ork_assets
import os
import obt.deco

deco = obt.deco.Deco()

def print_asset_info(cfgspc, catalog, fqid):
    """Print detailed information about a single asset"""
    asset_info = catalog.get_asset_info(fqid)
    merged_cfg = cfgspc.merged_config
    if not asset_info:
        print(deco.red(f"Asset not found: {fqid}"))
        return False
    
    print(f"{deco.yellow('Asset:')} {deco.cyan(fqid)}")
    print(f"  {deco.key('Type:')} {deco.val(asset_info.type)}")
    print(f"  {deco.key('Priority:')} {deco.val(str(asset_info.priority))}")
    print(f"  {deco.key('Platforms:')} {deco.val(', '.join(asset_info.platforms))}")
    
    if hasattr(asset_info, 'local_loc') and asset_info.local_loc:
        print(f"  {deco.key('Local location:')} {deco.val(asset_info.local_loc)}")
        resolved_path = merged_cfg.resolveLocalPath(asset_info.local_loc)
        print(f"  {deco.orange('Resolved Local:')} {deco.val(resolved_path)}")
        is_present = os.path.exists(resolved_path)
        decorator = deco.cyan if is_present else deco.red
        print(f"  {deco.key('Exists locally:')} {decorator('Yes' if is_present else 'No')}")
        if is_present:
          import hashlib
          md5 = hashlib.md5()
          is_pak = asset_info.type == 'asset_pak'
          if not is_pak:
            with open(resolved_path, 'rb') as f:
              for chunk in iter(lambda: f.read(4096), b""):
                  md5.update(chunk)
            md5 = md5.hexdigest()
            print(f"  {deco.key('MD5 checksum:')} {deco.val(md5)}")
    
    # Get remote location from namespace
    namespace_id = fqid.split('|')[0]
    remote_loc = cfgspc.getNamespaceRemoteLocation(namespace_id)
    if remote_loc:
        print(f"  {deco.key('Remote location (from namespace):')} {deco.val(remote_loc)}")
        resolved_location = merged_cfg.resolveRemoteLocation(remote_loc)
        if resolved_location:
            print(f"  {deco.orange('Resolved Download URL:')} {deco.val(resolved_location.download_url)}")
            print(f"  {deco.orange('Resolved Upload URL:')} {deco.val(resolved_location.upload_url)}")
    if hasattr(asset_info, 'filename') and asset_info.filename:
        print(f"  {deco.key('Filename:')} {deco.val(asset_info.filename)}")
    
    if hasattr(asset_info, 'size') and asset_info.size > 0:
        print(f"  {deco.key('Size:')} {deco.val(f'{asset_info.size:,} bytes')}")
    if hasattr(asset_info, 'content_hash') and asset_info.content_hash:
        print(f"  {deco.key('Content hash:')} {deco.val(asset_info.content_hash)}")
    if hasattr(asset_info, 'storage_hash') and asset_info.storage_hash:
        print(f"  {deco.key('Storage hash:')} {deco.val(asset_info.storage_hash)}")
        # Show encrypted file location
        cache_dir = catalog.cache_dir
        enc_path = os.path.join(cache_dir, 'enc', f"{asset_info.storage_hash}.enc")
        print(f"  {deco.magenta('Encrypted path:')} {deco.val(enc_path)}")
        enc_exists = os.path.exists(enc_path)
        decorator = deco.cyan if enc_exists else deco.red
        print(f"  {deco.key('Encrypted exists:')} {decorator('Yes' if enc_exists else 'No')}")
        if enc_exists:
            # Calculate MD5 of encrypted file
            import hashlib
            md5 = hashlib.md5()
            with open(enc_path, 'rb') as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    md5.update(chunk)
            enc_md5 = md5.hexdigest()
            print(f"  {deco.key('Encrypted MD5:')} {deco.val(enc_md5)}")
    if hasattr(asset_info, 'hash_algorithm') and asset_info.hash_algorithm:
        print(f"  {deco.key('Hash algorithm:')} {deco.val(asset_info.hash_algorithm)}")
    
    # Check if asset has dependencies
    if hasattr(asset_info, 'dependencies') and asset_info.dependencies:
        print(f"  {deco.key('Dependencies:')}")
        for dep in asset_info.dependencies:
            print(f"    {deco.cyan('-')} {deco.val(dep)}")
    
    # Check if asset is chunked
    if hasattr(asset_info, 'chunk_manifest') and asset_info.chunk_manifest:
        chunk_manifest = asset_info.chunk_manifest
        print(f"  {deco.key('Chunked:')} {deco.val('Yes')}")
        if hasattr(chunk_manifest, 'chunk_size'):
            print(f"    {deco.key('Chunk size:')} {deco.val(f'{chunk_manifest.chunk_size:,} bytes')}")
        if hasattr(chunk_manifest, 'chunks'):
            num_chunks = len(chunk_manifest.chunks)
            print(f"    {deco.key('Total chunks:')} {deco.val(str(num_chunks))}")
            if hasattr(chunk_manifest, 'total_size'):
                print(f"    {deco.key('Total size:')} {deco.val(f'{chunk_manifest.total_size:,} bytes')}")
            if hasattr(chunk_manifest, 'file_hash'):
                print(f"    {deco.key('File hash:')} {deco.val(str(chunk_manifest.file_hash))}")
            if hasattr(chunk_manifest, 'compression'):
                compression = chunk_manifest.compression
                # Handle CompressionType enum
                if hasattr(compression, 'name'):
                    compression_str = compression.name
                else:
                    compression_str = str(compression)
                print(f"    {deco.key('Compression:')} {deco.val(compression_str)}")
            if hasattr(chunk_manifest, 'is_encrypted'):
                print(f"    {deco.key('Encrypted:')} {deco.val('Yes' if chunk_manifest.is_encrypted else 'No')}")
            
            # Show first few and last chunk details
            if num_chunks > 0:
                print(f"    {deco.key('Chunk details:')}")
                chunks_to_show = min(3, num_chunks)  # Show first 3 chunks
                for i in range(chunks_to_show):
                    chunk = chunk_manifest.chunks[i]
                    print(f"      {deco.cyan(f'Chunk {i}:')}")
                    print(f"        {deco.key('Offset:')} {deco.val(f'{chunk.offset:,} bytes')}")
                    print(f"        {deco.key('Size:')} {deco.val(f'{chunk.size:,} bytes')}")
                    if hasattr(chunk, 'compressed_size'):
                        print(f"        {deco.key('Compressed size:')} {deco.val(f'{chunk.compressed_size:,} bytes')}")
                    print(f"        {deco.key('Hash:')} {deco.val(str(chunk.hash))}")
                
                if num_chunks > chunks_to_show:
                    print(f"      {deco.magenta(f'... and {num_chunks - chunks_to_show} more chunks')}")
                    # Show last chunk
                    last_chunk = chunk_manifest.chunks[-1]
                    print(f"      {deco.cyan(f'Chunk {num_chunks-1} (last):')}")
                    print(f"        {deco.key('Offset:')} {deco.val(f'{last_chunk.offset:,} bytes')}")
                    print(f"        {deco.key('Size:')} {deco.val(f'{last_chunk.size:,} bytes')}")
                    if hasattr(last_chunk, 'compressed_size'):
                        print(f"        {deco.key('Compressed size:')} {deco.val(f'{last_chunk.compressed_size:,} bytes')}")
                    print(f"        {deco.key('Hash:')} {deco.val(str(last_chunk.hash))}")
    
    # Check cache status
    if hasattr(asset_info, 'is_cached') and asset_info.is_cached:
        print(f"  {deco.key('Cached:')} {deco.val('Yes')}")
    else:
        print(f"  {deco.key('Cached:')} {deco.val('No')}")
    
    # Get the parent manifest info if available
    namespace_id = fqid.split('|')[0]
    manifest = catalog.get_manifest(namespace_id)
    if manifest:
        print(f"  {deco.key('Manifest namespace:')} {deco.val(manifest.namespace)}")
        print(f"  {deco.key('Manifest version:')} {deco.val(manifest.version)}")
    
    return True

def print_json_info(cfgspc, catalog, fqid):
    """Print asset information as JSON"""
    
    cfg_merged = cfgspc.merged_config
    asset_info = catalog.get_asset_info(fqid)
    
    if not asset_info:
        print(json.dumps({"error": f"Asset not found: {fqid}"}, indent=2))
        return False
    
    info_dict = {
        "asset_id": fqid,
        "type": asset_info.type,
        "priority": asset_info.priority,
        "platforms": asset_info.platforms
    }
    
    # Add optional fields if they exist
    optional_fields = [
        'local_loc', 'remote_loc', 'filename', 'size', 
        'content_hash', 'storage_hash', 'hash_algorithm'
    ]
    
    for field in optional_fields:
        if hasattr(asset_info, field):
            value = getattr(asset_info, field)
            if value:
                info_dict[field] = value

    info_dict['local(resolved)'] = cfg_merged.resolveLocalPath(asset_info.local_loc)
    
    if hasattr(asset_info, 'dependencies') and asset_info.dependencies:
        info_dict['dependencies'] = list(asset_info.dependencies)
    
    if hasattr(asset_info, 'is_cached'):
        info_dict['cached'] = asset_info.is_cached
    
    # Add manifest info
    namespace_id = fqid.split('|')[0]
    manifest = catalog.get_manifest(namespace_id)
    if manifest:
        info_dict['manifest'] = {
            'namespace': manifest.namespace,
            'version': manifest.version
        }
    
    print(json.dumps(info_dict, indent=2))
    return True

def main():
    parser = argparse.ArgumentParser(description='Get detailed information about an asset')
    parser.add_argument('asset_id', help='Fully qualified asset ID (namespace|asset_id)')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    args = parser.parse_args()
    
    core.coreappinit()
    
    # Create config space and catalog
    cfgspc, catalog = ork_assets.default_cfg_and_catalog()
        
    # Print asset info
    if args.json:
        success = print_json_info(cfgspc, catalog, args.asset_id)
    else:
        success = print_asset_info(cfgspc, catalog, args.asset_id)
    
    core.coreappexit()
    return 0 if success else 1

if __name__ == "__main__":
    exit(main())