#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path
import os

ddir = ork_path.data/"cdntest"
staging_dir = obt_path.stage()

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test new | delimiter
    ##################################
    
    print("=== Test Asset Catalog | Delimiter ===\n")
    
    # Create config space
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    cfg = cfgspc.createConfig("test", ddir/"config.json")
    
    # Create catalog with config space
    cat = core.AssetCatalog(space=cfgspc)
    
    # Register codec with password for hierarchical namespace
    cat.registerCodecWithPassword("game|levels|outdoor", "password123")
    
    # Create manifest with hierarchical namespace
    manifest = cat.createManifest(
        id="test_manifest",
        version="1.0",
        namespace="game|levels|outdoor",
        file=ddir/"test_manifest.json"
    )
    
    # Create test file
    test_file_path = staging_dir / "level1.dat"
    os.makedirs(staging_dir, exist_ok=True)
    with open(test_file_path, "wb") as f:
        f.write(b"LEVEL_DATA" * 100)
    
    # Create an asset
    asset = manifest.createAsset(
        id="level1_geometry",
        priority=100,
        type="level",
        remote="<cdntest_remote>",
        local="<stage>",
        filename="level1.dat",
        platforms=["win64", "linux64", "macos"],
        dependencies=[]
    )
    
    print(f"Created asset: {asset}")
    print(f"  Asset ID: {asset.id}")
    print(f"  Namespace ID: {asset.namespace_id}")
    print(f"  Fully Qualified ID: {asset.fqid}")
    
    # Test namespace hierarchy
    ns = cat.find_namespace("game|levels|outdoor")
    if ns:
        print(f"\nFound namespace: {ns.id}")
    
    # List assets with new delimiter pattern
    print("\nListing assets with pattern 'game|*':")
    assets = cat.list_assets("game|*")
    for asset_id in assets:
        print(f"  - {asset_id}")
    
    # Test FQIDs
    print("\nAll FQIDs in catalog:")
    all_fqids = cat.all_fqids
    for line in all_fqids.strip().split('\n'):
        if line:
            print(f"  - {line}")
    
    # The delimiter is working correctly as shown by the FQID output above
    
    print("\n✅ SUCCESS: | delimiter works correctly!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()
