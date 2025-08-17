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


# Module-level catalog cache to share state across build_assetpak calls
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


def build_assetpak(
    namespace,
    output,
    asset_id,
    source_dir=None,
    filters=None,
    priority=100,
    local_loc=None,
    version='1.0.0',
    catalog_file=None,
    cache_dir=None,
    key=None,
    strip_prefix=None,
    platforms=None,
    config_path=None,
    write_manifest=False,
    tar_root=None
):
    """
    Build asset_pak (tar archive) from directory using filters and add to Orkid catalog manifest
    
    Args:
        namespace: Asset namespace
        output: Output manifest file
        asset_id: Asset identifier
        source_dir: Archive root directory (required)
        filters: List of patterns to include relative to source_dir (required)
        priority: Priority (lower wins)
        local_loc: Local location template where TAR files are stored (required)
        version: Manifest version
        catalog_file: Catalog JSON file to load/update
        cache_dir: Cache directory for generated assets
        key: Encryption key (alternative to environment variable)
        strip_prefix: Prefix to strip from paths in TAR archive
        platforms: Target platforms (default: current platform)
        config_path: Additional config file to load
        write_manifest: Write updated manifest with new hashes to output file
        tar_root: Root directory in TAR for asset_pak (can be empty)
        
    Returns:
        dict with keys: asset_path, manifest_path, storage_hash, content_hash
    """
    # Validate required inputs
    if not source_dir:
        raise ValueError("source_dir is required")
    if not filters:
        raise ValueError("filters is required (list of patterns)")
    if not local_loc:
        raise ValueError("local_loc is required")
    
    # Ensure filters is a list
    if isinstance(filters, str):
        filters = [filters]
    
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
        
        # Load from global manifests (this populates the catalog and registers codecs)
        core.AssetCatalog.loadFromGlobalManifests(catalog)
        
        # Cache the catalog and config space
        _catalog_cache[cache_key] = {
            'catalog': catalog,
            'cfgspc': cfgspc
        }
    
    # If config_path is provided, load additional config
    if config_path:
        newcfg = core.AssetConfigSpace.loadConfigFromDisk(cfgspc, config_path)
        print(f"Loaded additional config from {config_path}")
    
    # Get config from catalog after loading additional configs
    config = catalog.merged_config
    
    # Register codec if key provided
    if key:
        catalog.registerCodecWithPassword(namespace, key)
    else:
        # Try to get key from environment
        env_key = f"ORKID_KEY_{namespace.upper()}"
        env_password = os.environ.get(env_key)
        if env_password:
            catalog.registerCodecWithPassword(namespace, env_password)
    
    # Resolve local location for manifest
    resolved_local = config.resolveLocalPath(str(local_loc))
    local_dir = Path(resolved_local)
    local_dir.mkdir(parents=True, exist_ok=True)
    
    # Resolve source directory
    resolved_source = config.resolveLocalPath(str(source_dir))
    source_path = Path(resolved_source)
    if not source_path.exists():
        raise ValueError(f"Source directory does not exist: {source_path}")
    
    print("############################################")
    print(f"## BUILDING ASSET_PAK: {asset_id}")
    print(f"## Source: {source_path}")
    print(f"## Filters: {filters}")
    if tar_root:
        print(f"## TAR Root: {tar_root}")
    print("############################################")
    
    # Get or create the manifest
    if cache_key in _catalog_cache and 'manifest' in _catalog_cache[cache_key]:
        manifest = _catalog_cache[cache_key]['manifest']
        print(f"Using cached manifest for namespace '{namespace}'")
    else:
        manifest = catalog.get_manifest(namespace)
        
        if manifest:
            print(f"Using existing manifest for namespace '{namespace}'")
        else:
            # Create new manifest using the catalog
            manifest = core.AssetCatalog.createManifest(
                catalog,
                f"{namespace}_manifest",
                version,
                namespace,
                output
            )
            print(f"Created new manifest for namespace '{namespace}'")
        
        # Cache the manifest
        _catalog_cache[cache_key]['manifest'] = manifest
    
    print("############################################")
    print(f"## PACKAGING")
    print("############################################")
    
    # Create asset using builder pattern - this will call repackage() internally
    # Pass tar_root (empty string if None) and filters to createAsset
    # Note: remote_loc removed - assets inherit from namespace
    asset_obj = core.AssetManifest.createAsset(
        manifest,
        asset_id,
        priority,
        'asset_pak',  # Always asset_pak for this function
        str(local_loc),
        platforms,
        [],  # No dependencies
        tar_root if tar_root is not None else "",  # Pass tar_root
        filters if filters else []  # Pass filters
    )
    
    # The asset should now be packaged (TAR created, encrypted, hashed)
    # The C++ code handles the actual packaging when createAsset is called
    
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
    print(f"  Content hash: {asset_obj.content_hash}")
    print(f"  Storage hash: {asset_obj.storage_hash}")
    print(f"  Size: {asset_obj.size}")
    if not write_manifest:
        print(f"  Note: Manifest not written to disk (use write_manifest=True to save)")
    
    # Check if encrypted file was created
    enc_path = obt_path.stage() / "assetcache" / "enc" / f"{asset_obj.storage_hash}.enc"
    if enc_path.exists():
        print(f"✓ Encrypted file created: {enc_path}")
    else:
        print(f"⚠ Encrypted file not found at: {enc_path}")
    
    print(f"\nTo upload this asset, use: ork.asset.catalog.upload.py --namespace {namespace}")
    
    return {
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