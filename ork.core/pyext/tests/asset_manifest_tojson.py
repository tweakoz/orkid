#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
import json
import tempfile
import os

ddir = ork_path.data/"cdntest"

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test AssetManifest.toJson()
    ##################################
    
    print("=== Testing AssetManifest.toJson() ===\n")
    
    # Create temp directory for test files
    with tempfile.TemporaryDirectory() as temp_dir:
        print(f"Using temp directory: {temp_dir}")
        
        # Create config space and catalog
        cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
        cfg1 = cfgspc.createConfig(id="test1", file=core.Path(temp_dir) / "config.json")
        cfg1.addRemoteLocation(id="cdntest_remote", loc="https://localhost:8443")
        cfg1.addNamespace("cdntest", "api_key", "cdntest_remote")
        cfg1.addLocalLocation(id="stage", loc="<stage>")
        cfg1.addLocalLocation(id="cache", loc=temp_dir)
        
        cat = core.AssetCatalog(space=cfgspc)
        cat.cache_dir = os.path.join(temp_dir, "assetcache")
        
        # Create test files
        # Create my_local_asset_dir/my_asset_file.txt
        local_asset_dir = os.path.join(temp_dir, "my_local_asset_dir")
        os.makedirs(local_asset_dir, exist_ok=True)
        with open(os.path.join(local_asset_dir, "my_asset_file.txt"), 'w') as f:
            f.write("This is test content for the asset file.")
        
        # Create configs/settings.json
        configs_dir = os.path.join(temp_dir, "configs")
        os.makedirs(configs_dir, exist_ok=True)
        with open(os.path.join(configs_dir, "settings.json"), 'w') as f:
            f.write('{"test": "config"}')
        
        # Create manifest
        m1 = cat.createManifest(
            id="cdntest",
            version="1.0.0",
            namespace="cdntest",
            file = core.Path(temp_dir) / "catalog.json")
        
        # Create multiple assets
        a1 = m1.createAsset(
            id="user_id",
            priority=100,
            type="text",
            remote = "<cdntest_remote>/my_remote_asset_dir",
            local = temp_dir,
            filename="my_local_asset_dir/my_asset_file.txt",
            platforms=["mac", "linux"],
            dependencies = ["fqid1", "fqid2"]
        )
        
        a2 = m1.createAsset(
            id="config_data",
            priority=200,
            type="json",
            remote = "<cdntest_remote>/configs",
            local = temp_dir,
            filename="configs/settings.json",
            platforms=["mac", "linux", "win32"],
            dependencies = []
        )
        
        # Test AssetManifest.toJson()
        print("=== AssetManifest.toJson() ===")
        manifest_json = m1.toJson()
        print(manifest_json)
        
        # Parse and verify JSON structure
        parsed = json.loads(manifest_json)
        
        # Verify manifest fields
        assert parsed["id"] == "cdntest", f"Expected id='cdntest', got '{parsed['id']}'"
        assert "uuid" in parsed, "Expected uuid field in manifest"
        assert parsed["namespace"] == "cdntest", f"Expected namespace='cdntest', got '{parsed['namespace']}'"
        assert parsed["version"] == "1.0.0", f"Expected version='1.0.0', got '{parsed['version']}'"
        assert "assets" in parsed, "Expected assets field in manifest"
        
        # Verify assets
        assets = parsed["assets"]
        assert len(assets) == 2, f"Expected 2 assets, got {len(assets)}"
        assert "user_id" in assets, "Expected 'user_id' asset"
        assert "config_data" in assets, "Expected 'config_data' asset"
        
        # Verify user_id asset
        user_asset = assets["user_id"]
        assert user_asset["type"] == "text", f"Expected type='text', got '{user_asset['type']}'"
        assert user_asset["priority"] == 100, f"Expected priority=100, got {user_asset['priority']}"
        assert "content_hash" in user_asset, "Expected content_hash in user_id asset"
        assert "storage_hash" in user_asset, "Expected storage_hash in user_id asset"
        
        # Verify config_data asset
        config_asset = assets["config_data"]
        assert config_asset["type"] == "json", f"Expected type='json', got '{config_asset['type']}'"
        assert config_asset["priority"] == 200, f"Expected priority=200, got {config_asset['priority']}"
        assert "content_hash" in config_asset, "Expected content_hash in config_data asset"
        assert "storage_hash" in config_asset, "Expected storage_hash in config_data asset"
        
        print("\n✓ AssetManifest.toJson() works correctly!")
        
        print("\n✅ SUCCESS: AssetManifest.toJson() test passed!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()