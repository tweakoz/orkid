#!/usr/bin/env ork.python

################################################################################

import sys, argparse, os, time
import concurrent.futures
from threading import Lock
from orkengine import core

################################################################################
# Parallel fetcher class
################################################################################

class ParallelFetcher:
    def __init__(self, catalog, max_workers=4, disable_cache=False):
        self.catalog = catalog
        self.max_workers = max_workers
        self.disable_cache = disable_cache
        self.completed = 0
        self.total = 0
        self.lock = Lock()
        self.failed_assets = []
        
    def fetch_single(self, asset_id):
        """Fetch a single asset (runs in thread)"""
        try:
            result = self.catalog.fetch(asset_id, decrypt=True, disable_cache=self.disable_cache)
            
            with self.lock:
                self.completed += 1
                current = self.completed
                
            if result and result.succeeded:
                print(f"  [{current}/{self.total}] ✓ {asset_id} ({result.bytes_downloaded} bytes)")
                return (asset_id, True, result)
            else:
                error = result.error_detail if result else "Unknown error"
                print(f"  [{current}/{self.total}] ✗ {asset_id}: {error}")
                return (asset_id, False, error)
                
        except Exception as e:
            with self.lock:
                self.completed += 1
                current = self.completed
            print(f"  [{current}/{self.total}] ✗ {asset_id}: {str(e)}")
            return (asset_id, False, str(e))
    
    def fetch_batch(self, asset_ids):
        """Fetch multiple assets in parallel"""
        self.total = len(asset_ids)
        self.completed = 0
        results = []
        
        print(f"\nFetching {self.total} assets with {self.max_workers} workers...")
        if self.disable_cache:
            print("Cache disabled - downloading from remote")
        print("-" * 50)
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            # Submit all tasks
            futures = {executor.submit(self.fetch_single, asset_id): asset_id 
                      for asset_id in asset_ids}
            
            # Process results as they complete
            for future in concurrent.futures.as_completed(futures):
                asset_id, success, data = future.result()
                results.append((asset_id, success, data))
                if not success:
                    self.failed_assets.append(asset_id)
        
        return results

################################################################################
# Helper functions
################################################################################

def resolve_assets_to_fetch(catalog, patterns, namespaces):
    """Resolve patterns and namespaces to concrete asset IDs"""
    assets_to_fetch = set()
    
    # Handle namespace requests (fetch all in namespace)
    if namespaces:
        for ns in namespaces:
            # Get all assets in this namespace
            ns_assets = catalog.list_assets(f"{ns}|*")
            assets_to_fetch.update(ns_assets)
            print(f"Found {len(ns_assets)} assets in namespace '{ns}'")
    
    # Handle pattern requests
    if patterns:
        for pattern in patterns:
            # Direct pattern matching using catalog's built-in support
            matching = catalog.list_assets(pattern)
            assets_to_fetch.update(matching)
            print(f"Pattern '{pattern}' matched {len(matching)} assets")
    
    # Must specify something
    if not patterns and not namespaces:
        print("Error: Must specify -p patterns or -n namespaces")
        sys.exit(1)
    
    return sorted(list(assets_to_fetch))

def fetch_assets_sequential(catalog, asset_ids):
    """Fetch multiple assets concurrently using async API"""
    total = len(asset_ids)
    success_count = 0
    failed_assets = []
    
    print(f"\nFetching {total} assets concurrently...")
    print("-" * 50)
    
    # Enqueue all assets for concurrent fetching
    futures = []
    for asset_id in asset_ids:
        future = catalog.fetchAsync(asset_id)
        futures.append((asset_id, future))
    
    print(f"Enqueued {total} assets for concurrent download")
    print("-" * 50)
    
    # Wait for all futures to complete
    for asset_id, future in futures:
        try:
            OK = future.wait()  # Block until this asset is ready
            
            if OK:
                print(f"  ✓ {asset_id} OK")
                success_count += 1
            else:
                print(f"  ✗ {asset_id}: ERROR")
                failed_assets.append(asset_id)
                
        except Exception as e:
            print(f"  ✗ {asset_id}: {e}")
            failed_assets.append(asset_id)
    
    # Summary
    print("-" * 50)
    print(f"\nCompleted: {success_count}/{total} successful")
    
    if failed_assets:
        print(f"\nFailed assets ({len(failed_assets)}):")
        for asset in failed_assets:
            print(f"  - {asset}")
        return False
    
    return True

def fetch_assets_parallel(catalog, asset_ids, num_workers=4, disable_cache=False):
    """Fetch assets with parallel downloads"""
    fetcher = ParallelFetcher(catalog, max_workers=num_workers, disable_cache=disable_cache)
    results = fetcher.fetch_batch(asset_ids)
    
    # Summary
    success_count = sum(1 for _, success, _ in results if success)
    print("-" * 50)
    print(f"\nCompleted: {success_count}/{len(asset_ids)} successful")
    
    if fetcher.failed_assets:
        print(f"\nFailed assets ({len(fetcher.failed_assets)}):")
        for asset in fetcher.failed_assets:
            print(f"  - {asset}")
        return False
    
    return True

################################################################################
# Main
################################################################################

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Fetch assets from catalog')
    parser.add_argument("-p", '--pattern', action='append', dest='patterns',
                       help='Asset pattern(s) to fetch (can specify multiple)')
    parser.add_argument("-n", '--namespace', action='append', dest='namespaces', 
                       help='Fetch all assets from namespace(s)')
    parser.add_argument("--parallel", type=int, default=1,
                       help='Number of parallel downloads (default: 1)')

    args = parser.parse_args()

    # Initialize
    core.coreappinit(name="CatalogFetcher")
    catalog = core.AssetCatalog.instance

    # Resolve what to fetch
    assets = resolve_assets_to_fetch(catalog, args.patterns, args.namespaces)

    if not assets:
        print("No assets matched the specified patterns/namespaces")
        sys.exit(1)

    # Fetch them
    if args.parallel > 1:
        success = fetch_assets_parallel(catalog, assets, args.parallel)
    else:
        success = fetch_assets_sequential(catalog, assets)

    core.coreappexit()
    sys.exit(0 if success else 1)