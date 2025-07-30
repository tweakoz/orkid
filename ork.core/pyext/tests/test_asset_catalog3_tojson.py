#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
import json

ddir = ork_path.data/"cdntest"

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test toJson() methods
    ##################################
    
    print("=== Testing toJson() methods ===\n")
    
    # Create config space and catalog
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    cfg1 = cfgspc.createConfig(id="test1", file=ddir/"config.json")
    cfg1.addRemoteLocation(id="cdntest_remote", loc="https://localhost:8443")
    cfg1.addNamespace("cdntest", "api_key", "cdntest_remote")
    cfg1.addLocalLocation(id="stage", loc="<stage>")
    cfg1.addLocalLocation(id="cache", loc="<assetcache>")
    
    cat = core.AssetCatalog(space=cfgspc)
    
    # Create manifest
    m1 = cat.createManifest(
        id="cdntest",
        version="1.0.0",
        namespace="cdntest",
        file = ddir/"catalog.json")
    
    # Create asset
    a1 = m1.createAsset(
        id="user_id",
        priority=100,
        type="text",
        remote = "<cdntest_remote>/my_remote_asset_dir",
        local = "<cache>/my_local_asset_dir",
        filename="my_asset_file.txt",
        platforms=["mac", "linux"],
        dependencies = ["fqid1", "fqid2"]
    )
    
    # Test AssetEntry.toJson()
    print("=== AssetEntry.toJson() ===")
    asset_json = a1.toJson()
    print(asset_json)
    
    # Parse and verify JSON structure
    parsed = json.loads(asset_json)
    
    # Verify expected fields
    assert parsed["id"] == "user_id", f"Expected id='user_id', got '{parsed['id']}'"
    assert parsed["namespace"] == "cdntest", f"Expected namespace='cdntest', got '{parsed['namespace']}'"
    assert parsed["type"] == "text", f"Expected type='text', got '{parsed['type']}'"
    assert parsed["priority"] == 100, f"Expected priority=100, got {parsed['priority']}"
    assert parsed["filename"] == "my_asset_file.txt", f"Expected filename='my_asset_file.txt', got '{parsed['filename']}'"
    assert parsed["platforms"] == ["mac", "linux"], f"Expected platforms=['mac', 'linux'], got {parsed['platforms']}"
    assert parsed["dependencies"] == ["fqid1", "fqid2"], f"Expected dependencies=['fqid1', 'fqid2'], got {parsed['dependencies']}"
    
    print("\n✓ AssetEntry.toJson() works correctly!")
    
    print("\n✅ SUCCESS: All toJson() tests passed!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()