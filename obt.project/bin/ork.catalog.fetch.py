#!/usr/bin/env ork.python

import sys, argparse, os
import concurrent.futures
from threading import Lock
from orkengine import core

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
        self.total = len(asset_ids)
        self.completed = 0
        results = []

        print(f"\nFetching {self.total} assets with {self.max_workers} workers...")
        if self.disable_cache:
            print("Cache disabled - downloading from remote")
        print("-" * 50)

        with concurrent.futures.ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            futures = {executor.submit(self.fetch_single, asset_id): asset_id
                      for asset_id in asset_ids}

            for future in concurrent.futures.as_completed(futures):
                asset_id, success, data = future.result()
                results.append((asset_id, success, data))
                if not success:
                    self.failed_assets.append(asset_id)

        return results

def resolve_assets_to_fetch(catalog, patterns, namespaces):
    assets_to_fetch = set()

    if namespaces:
        for ns in namespaces:
            ns_assets = catalog.list_assets(f"{ns}|*")
            assets_to_fetch.update(ns_assets)
            print(f"Found {len(ns_assets)} assets in namespace '{ns}'")

    if patterns:
        for pattern in patterns:
            matching = catalog.list_assets(pattern)
            assets_to_fetch.update(matching)
            print(f"Pattern '{pattern}' matched {len(matching)} assets")

    if not patterns and not namespaces:
        print("Error: Must specify -p patterns or -n namespaces")
        sys.exit(1)

    return sorted(list(assets_to_fetch))

def fetch_assets_async(app, catalog, asset_ids):
    """Fetch assets using mainThreadLoop for async processing"""
    total = len(asset_ids)

    print(f"\nFetching {total} assets...")
    print("-" * 50)

    # State tracking
    state = {
        'pending': [],  # (asset_id, fetch_request) tuples
        'success': [],
        'failed': []
    }

    # Queue all async fetches
    for asset_id in asset_ids:
        fetch_req = catalog.fetchAsync(asset_id)
        state['pending'].append((asset_id, fetch_req))

    print(f"Enqueued {total} assets for download")
    print("-" * 50)

    def on_iter():
        # Check each pending fetch
        still_pending = []
        for asset_id, fetch_req in state['pending']:
            if fetch_req.completed:
                ok = fetch_req.succeeded  # Check success status
                if ok:
                    print(f"  ✓ {asset_id} OK")
                    state['success'].append(asset_id)
                else:
                    print(f"  ✗ {asset_id}: ERROR")
                    state['failed'].append(asset_id)
            else:
                still_pending.append((asset_id, fetch_req))

        state['pending'] = still_pending

        # Exit when all done
        if not state['pending']:
            app.requestExit()

    # Run main loop until all complete or Ctrl-C
    app.mainThreadLoop(on_iter=on_iter)

    # Print results
    success_count = len(state['success'])
    print("-" * 50)
    print(f"\nCompleted: {success_count}/{total} successful")

    if state['failed']:
        print(f"\nFailed assets ({len(state['failed'])}):")
        for asset in state['failed']:
            print(f"  - {asset}")
        return False

    return True

def fetch_assets_parallel(catalog, asset_ids, num_workers=4, disable_cache=False):
    """Fetch assets using Python thread pool for parallel downloads"""
    fetcher = ParallelFetcher(catalog, max_workers=num_workers, disable_cache=disable_cache)
    results = fetcher.fetch_batch(asset_ids)

    success_count = sum(1 for _, success, _ in results if success)
    print("-" * 50)
    print(f"\nCompleted: {success_count}/{len(asset_ids)} successful")

    if fetcher.failed_assets:
        print(f"\nFailed assets ({len(fetcher.failed_assets)}):")
        for asset in fetcher.failed_assets:
            print(f"  - {asset}")
        return False

    return True

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Fetch assets from catalog')
    parser.add_argument("-p", '--pattern', action='append', dest='patterns',
                       help='Asset pattern(s) to fetch (can specify multiple)')
    parser.add_argument("-n", '--namespace', action='append', dest='namespaces',
                       help='Fetch all assets from namespace(s)')
    parser.add_argument("--parallel", type=int, default=1,
                       help='Number of parallel downloads (default: 1)')

    args = parser.parse_args()

    app = core.Application.create(name="CatalogFetcher", std_asset_catalog=True)
    catalog = core.AssetCatalog.instance

    assets = resolve_assets_to_fetch(catalog, args.patterns, args.namespaces)

    if not assets:
        print("No assets matched the specified patterns/namespaces")
        sys.exit(1)

    if args.parallel > 1:
        success = fetch_assets_parallel(catalog, assets, args.parallel)
    else:
        success = fetch_assets_async(app, catalog, assets)

    sys.exit(0 if success else 1)
