#!/usr/bin/env ork.python

################################################################################

import sys, argparse, os
from orkengine import core

core.coreappinit()

parser = argparse.ArgumentParser(description='assetpak fetcher')
parser.add_argument("-p", '--pack', type=str, help='asset ID as namespace|asset_id (e.g., singularity|std)', required=True)
parser.add_argument("-f", '--force', action='store_true', help='Force download even if cached')
args = vars(parser.parse_args())

pack = args['pack']

if pack is None:
  print("must supply pack")
  sys.exit(0)

# Create config space and catalog
cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
catalog = core.AssetCatalog(space=cfgspc)

# Load from global manifests
core.AssetCatalog.loadFromGlobalManifests(catalog)

# Extract namespace from pack identifier
namespace = pack.split('|')[0] if '|' in pack else pack

# Fetch the asset
try:
    # Debug: check if asset exists
    if catalog.has_asset(pack):
        print(f"Found asset: {pack}")
        asset_info = catalog.get_asset_info(pack)
        if asset_info:
            print(f"Asset info: {asset_info}")
    else:
        print(f"Asset not found: {pack}")
        print(f"Available assets: {catalog.list_assets(namespace + '|*')}")
    
    result = catalog.get(pack, decrypt=True)
    
    if result and result.is_success():
        print(f"✓ Successfully fetched {pack}")
        print(f"  Downloaded: {result.bytes_downloaded} bytes")
        print(f"  Download time: {result.download_time:.2f}s")
        print(f"  Processing time: {result.processing_time:.2f}s")
        fetch_count = 1
    else:
        print(f"✗ Failed to fetch {pack}")
        if result:
            print(f"  Error: {result.error_detail}")
            print(f"  Status: {result.status}")
        fetch_count = 0
except Exception as e:
    print(f"✗ Error fetching {pack}: {e}")
    import traceback
    traceback.print_exc()
    fetch_count = 0

# Exit with error if nothing was fetched
if fetch_count == 0:
    sys.exit(1)

core.coreappexit()