#!/usr/bin/env python3
################################################################################
# Orkid Asset Management Module - Using C++ Python Bindings
################################################################################

import os
import json
import hashlib
import tarfile
import tempfile
import subprocess
import platform
import fnmatch
from pathlib import Path
from datetime import datetime
from obt import crypt, path as obt_path
from orkengine import core


# Module-level catalog cache to share state across package_asset calls
_catalog_cache = {}


def get_current_platform():
    """Get the current platform name"""
    system = platform.system()
    if system == "Darwin":
        return "mac"
    elif system == "Linux":
        return "linux"
    else:
        raise ValueError(f"Unsupported platform: {system}")


def package_asset(
    namespace,
    output,
    asset_id,
    priority=100,
    remote_loc=None,
    local_loc=None,
    version='1.0.0',
    catalog_file=None,
    asset_pak=None,
    asset=None,
    cache_dir=None,
    merge=None,
    dependencies=None,
    key=None,
    filename=None,
    strip_leading=False,
    filter=None,
    platforms=None,
    config_path=None,
    write_manifest=False
):
    # TODO: Currently each package_asset call creates its own catalog instance,
    # which means when packaging multiple assets that share a manifest, each call
    # only updates its own asset's data. This requires write_manifest=True for
    # each call to persist changes. A better approach would be to support batch
    # operations or share catalog state across calls.
    """
    Package assets (encrypt/compress) and add them to Orkid catalog manifests
    
    Args:
        namespace: Asset namespace
        output: Output manifest file
        asset_id: Asset identifier
        priority: Priority (lower wins)
        remote_loc: Remote location template (e.g., <orkid_cdn>)
        local_loc: Local location template
        version: Manifest version
        catalog_file: Catalog JSON file to load/update
        asset_pak: Create asset_pak from directory (mutually exclusive with asset)
        asset: Create asset from file (mutually exclusive with asset_pak)
        cache_dir: Cache directory for generated assets
        merge: For asset_pak: merge with existing
        dependencies: Dependencies (namespace|asset_id)
        key: Encryption key (alternative to environment variable)
        filename: Override filename in manifest
        strip_leading: Strip base directory from tar archive paths
        filter: Wildcard pattern to filter files (only for asset_pak)
        platforms: Target platforms (default: current platform)
        write_manifest: Write updated manifest with new hashes to output file
        
    Returns:
        dict with keys: asset_path, manifest_path
    """
    # Validate inputs
    if not remote_loc:
        raise ValueError("remote_loc is required")
    if not local_loc:
        raise ValueError("local_loc is required")
    
    # Validate mutually exclusive options
    if asset_pak and asset:
        raise ValueError("Cannot specify both --asset-pak and --asset")
    if not asset_pak and not asset:
        raise ValueError("Must specify either --asset-pak or --asset")
    
    # Set default platforms if not specified
    if not platforms:
        platforms = [get_current_platform()]
    
    # Check if we have a cached catalog for this namespace
    cache_key = namespace
    if cache_key in _catalog_cache:
        print(f"Using cached catalog for namespace '{namespace}'")
        catalog = _catalog_cache[cache_key]['catalog']
        cfgspc = _catalog_cache[cache_key]['cfgspc']
    else:
        # Initialize core for catalog API
        core.coreappinit()
        
        # Create empty catalog first
        cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
        catalog = core.AssetCatalog(space=cfgspc)
        
        # Phase 1: Config Space Identity Check
        print(f"DEBUG: cfgspc object id: {id(cfgspc)}")
        print(f"DEBUG: catalog object id: {id(catalog)}")
        
        # Load from global manifests (this populates the "default" catalog)
        core.AssetCatalog.loadFromGlobalManifests(catalog)
        
        # Cache the catalog and config space
        _catalog_cache[cache_key] = {
            'catalog': catalog,
            'cfgspc': cfgspc
        }
    
    # If config_path is provided, load additional config
    if config_path:
        # Use the static method which automatically merges into the config space
        newcfg = core.AssetConfigSpace.loadConfigFromDisk(cfgspc, config_path)
        print(f"DEBUG: Config loaded from {config_path}")
        
        # Test config loaded
        
        # Test configuration loaded successfully
        
    # Get config from catalog after loading additional configs
    config = catalog.merged_config
    
    # Debug: Use API to inspect what's actually in the merged config
    if config_path:
        print(f"DEBUG: Merged config destinations: {config.destinations}")
        print(f"DEBUG: Merged config JSON:\n{config.toJson()}")
        
        # Test template resolution
        test_resolution = config.resolveLocalPath("<test_stage>/levels")
        print(f"DEBUG: Template '<test_stage>/levels' resolves to: '{test_resolution}'")
        
        # Try to resolve the path here before it gets to createAsset
        print(f"DEBUG: About to call resolveLocalPath with local_loc='{local_loc}'")
        actual_resolved = config.resolveLocalPath(local_loc)
        print(f"DEBUG: Resolved path: '{actual_resolved}'") 
    # TODO! assert namespace exists in catalog
    
    # Register codec if key provided
    if key:
        catalog.registerCodecWithPassword(namespace, key)
    else:
        # Try to get key from environment
        env_key = f"ORKID_KEY_{namespace.upper()}"
        env_password = os.environ.get(env_key)
        if env_password:
            catalog.registerCodecWithPassword(namespace, env_password)
    
    # For now, we cannot resolve location templates through the catalog API
    # The catalog needs a method like resolveLocalLocation(template_str) -> Path
    # Until that API exists, we must require resolved paths
    resolved_local = config.resolveLocalPath(local_loc)
    local_dir = Path(resolved_local)
    assert( local_dir.exists() ), f"Local location '{local_dir}' does not exist"
        
    # Determine type and prepare the unencrypted asset
    if asset_pak:

        print("############################################")
        print(f"## COPYING ASSET_PAK: {local_dir}")
        print("############################################")

        asset_type = 'asset_pak'
        # Resolve template path first before checking if it's a directory
        resolved_asset_pak = config.resolveLocalPath(asset_pak)
        source_dir = Path(resolved_asset_pak)
        if not source_dir.is_dir():
            raise ValueError(f"asset_pak source must be a directory: {source_dir}")
        
        # For asset_pak, the C++ API expects a directory at local_loc
        # The C++ packFromLocal will create TAR from this directory
        if filename:
            # Use the provided filename (should be like "test.tar")
            final_filename = filename
        else:
            final_filename = f"{namespace}_{asset_id}.tar"
        
        # Extract directory name from tar filename for copying source
        pak_dirname = final_filename
        if pak_dirname.endswith('.tar'):
            pak_dirname = pak_dirname[:-4]
        
        # Check if source and dest are the same to avoid recursive copying
        dest_dir = local_dir / pak_dirname
        needs_cleanup = False
        
        if source_dir == local_dir:
            # Source and dest in same location - need to handle carefully
            print(f"## Source in local_loc, creating temporary subdirectory")
            needs_cleanup = True
            
            # Remove existing subdirectory if it exists (from previous run)
            if dest_dir.exists():
                import shutil
                shutil.rmtree(dest_dir)
        else:
            # Normal case - copy from different location
            if dest_dir.exists():
                import shutil
                shutil.rmtree(dest_dir)
            
            print(f"## Copying directory to -> {dest_dir}")
        
        # If filter is specified, copy only matching files
        if filter:
            dest_dir.mkdir(parents=True, exist_ok=True)
            copied_count = 0
            
            # When source == dest, we need to skip subdirectories to avoid recursion
            for file in os.listdir(source_dir):
                file_path = source_dir / file
                if file_path.is_file() and fnmatch.fnmatch(file, filter):
                    import shutil
                    shutil.copy2(str(file_path), str(dest_dir / file))
                    copied_count += 1
            
            print(f"✓ Copied directory ({copied_count} files matching '{filter}')")
        else:
            import shutil
            if source_dir == local_dir:
                # Special case: copy only files, not subdirectories
                dest_dir.mkdir(parents=True, exist_ok=True)
                for item in os.listdir(source_dir):
                    item_path = source_dir / item
                    if item_path.is_file():
                        shutil.copy2(str(item_path), str(dest_dir / item))
            else:
                shutil.copytree(str(source_dir), str(dest_dir))
            print(f"✓ Copied directory ({len(list(dest_dir.glob('*')))} files)")
        
        # The C++ API will create {local_loc}/{filename} TAR from {local_loc}/{pak_dirname} directory
        
    else:

        print("############################################")
        print(f"## DOING SINGLE ASSET: {asset}")
        print("############################################")

        asset_type = 'asset'
        source_file = Path(asset)
        if not source_file.is_file():
            raise ValueError(f"asset source must be a file: {source_file}")
        
        # Determine filename
        if filename:
            final_filename = filename
            # Remove .enc extension if present since we need unencrypted name
            if final_filename.endswith('.enc'):
                final_filename = final_filename[:-4]
        else:
            # Keep original extension for single files
            orig_ext = source_file.suffix
            final_filename = f"{namespace}_{asset_id}{orig_ext}"
        
        # Copy file to expected location
        dest_path = local_dir / final_filename
        print(f"Copying file to: {dest_path}")
        
        import shutil
        shutil.copy2(str(source_file), str(dest_path))
        print(f"✓ Copied file: {dest_path} ({dest_path.stat().st_size} bytes)")

    # Get or create the manifest
    # Check if we have a cached manifest for this namespace
    if cache_key in _catalog_cache and 'manifest' in _catalog_cache[cache_key]:
        manifest = _catalog_cache[cache_key]['manifest']
        print(f"Using cached manifest for namespace '{namespace}'")
    else:
        manifest = catalog.get_manifest(namespace)
        
        if manifest:
            # Use existing manifest
            print(f"Using existing manifest for namespace '{namespace}'")
        else:
            # Create new manifest using the catalog
            manifest = core.AssetCatalog.createManifest(
                catalog,
                f"{namespace}_manifest",  # manifest ID
                version,
                namespace,
                output
            )
            print(f"Created new manifest for namespace '{namespace}'")
        
        # Cache the manifest
        _catalog_cache[cache_key]['manifest'] = manifest
    
    # Format dependencies for new API (as lists, not dicts)
    if not dependencies:
        dependencies = []
    
    print("############################################")
    print(f"## PACKAGING")
    print("############################################")
    # Create asset using builder pattern - this will call repackage() internally
    asset_obj = core.AssetManifest.createAsset(
        manifest,
        asset_id,
        priority,
        asset_type,
        remote_loc,
        local_loc,
        final_filename,
        platforms,
        dependencies
    )
    
    # If merge is specified for asset_pak
    if asset_type == "asset_pak" and merge is not None:
        asset_obj._merge = merge
    
    # Clean up temporary directory if we created one for asset_pak
    if asset_type == "asset_pak" and 'needs_cleanup' in locals() and needs_cleanup:
        # We created a temporary subdirectory for filtering, clean it up
        temp_dir = local_dir / pak_dirname
        if temp_dir.exists():
            import shutil
            shutil.rmtree(temp_dir)
            print(f"✓ Cleaned up temporary directory: {temp_dir}")
    
    # Set output path (always needed for return value)
    output_path = Path(output)
    
    # Write manifest to file if requested
    if write_manifest:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        
        manifest_json = manifest.toJson()
        with open(output_path, 'w') as f:
            f.write(manifest_json)
        print(f"✓ Manifest written to: {output}")
    
    print(f"✓ Created asset: {asset_obj}")
    print(f"  Type: {asset_obj.type}")
    print(f"  Filename: {asset_obj.filename}")
    print(f"  Content hash: {asset_obj.content_hash}")
    print(f"  Storage hash: {asset_obj.storage_hash}")
    print(f"  Size: {asset_obj.size}")
    if not write_manifest:
        print(f"  Note: Manifest not written to disk (use write_manifest=True to save)")
    
    # Check if encrypted file was created
    enc_path = obt_path.stage() / "assetcache" / "enc" / f"{asset_obj.storage_hash}.enc"
    if enc_path.exists():
        print(f"✓ Encrypted file created: {enc_path}")
    
    print(f"\nTo upload this asset, use: ork.asset.catalog.upload.py --namespace {namespace}")
    
    # Don't call coreappexit if we're using cached catalogs
    # Let the process cleanup handle it
    
    # For asset_pak, the asset_path is the directory, not the TAR file
    if asset_type == 'asset_pak':
        asset_path = str(local_dir / pak_dirname)
    else:
        asset_path = str(local_dir / final_filename)
    
    return {
        'asset_path': asset_path,
        'manifest_path': str(output_path),
        'storage_hash': asset_obj.storage_hash,
        'content_hash': asset_obj.content_hash
    }


def clear_catalog_cache():
    """Clear the module-level catalog cache"""
    global _catalog_cache
    _catalog_cache.clear()
    print("Catalog cache cleared")


def default_cfg_and_catalog():
    """Create default config space and catalog"""
    core.coreappinit()
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    catalog = core.AssetCatalog(space=cfgspc)
    core.AssetCatalog.loadFromGlobalManifests(catalog)
    return cfgspc, catalog