#!/usr/bin/env python3
################################################################################
# Orkid Asset Management Module - Using C++ Python Bindings
################################################################################

import os
from pathlib import Path
from obt import path
from orkengine import core


def fetch_pak(pack_identifier, progress_callback=None, complete_callback=None, force_download=False):
    """Fetch asset pack(s) by identifier using C++ AssetFetcher
    
    Args:
        pack_identifier: Can be either:
            - Full asset ID: "namespace.asset_id" 
            - Namespace only: "namespace" (fetches all assets in namespace)
        progress_callback: Optional callback(asset_id, downloaded, total)
        complete_callback: Optional callback(asset_id, success)
        force_download: Force download even if cached
    
    Returns:
        Number of assets fetched
    """
    # Create fetcher with download manager
    download_mgr = core.DownloadManager()
    
    # Disable cache if force download requested
    if force_download:
        download_mgr.setCacheEnabled(False)
        print("Force download enabled - cache disabled")
    
    fetcher = core.AssetFetcher(download_mgr)
    
    # Set up callbacks if provided
    if progress_callback:
        fetcher.on_asset_progress(progress_callback)
    
    if complete_callback:
        fetcher.on_asset_complete(complete_callback)
    
    # Fetch the assets
    fetch_count = fetcher.fetch_pak(pack_identifier)
    
    return fetch_count


def list_assets():
    """List all available assets"""
    fetcher = core.AssetFetcher()
    fetcher.reload()
    return fetcher.get_assets()


def get_asset_config():
    """Get the merged asset configuration"""
    fetcher = core.AssetFetcher()
    fetcher.reload()
    return fetcher.get_config()


def get_asset_info(asset_id):
    """Get information about a specific asset
    
    Args:
        asset_id: Full asset ID (namespace.asset_id)
        
    Returns:
        AssetEntry or None if not found
    """
    assets = list_assets()
    return assets.get(asset_id)