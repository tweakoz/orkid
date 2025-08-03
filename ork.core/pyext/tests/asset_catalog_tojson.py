#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
import json
import tempfile
import os

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test AssetCatalog.toJson()
    ##################################
    
    print("=== Testing AssetCatalog.toJson() ===\n")
    
    # Create temp directory for test files
    with tempfile.TemporaryDirectory() as temp_dir:
        print(f"Using temp directory: {temp_dir}")
        
        # Create test asset files
        asset1_file = os.path.join(temp_dir, "my_asset_file.txt")
        with open(asset1_file, 'w') as f:
            f.write("Test asset content 1")
        
        models_dir = os.path.join(temp_dir, "models")
        os.makedirs(models_dir, exist_ok=True)
        asset2_file = os.path.join(models_dir, "player.gltf")
        with open(asset2_file, 'w') as f:
            f.write("Test model content")
        
        # Create config space and catalog  
        cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
        cfg1 = cfgspc.createConfig(id="test1", file=core.Path(temp_dir) / "config.json")
        cfg1.addRemoteLocation(id="cdntest_remote", loc="https://localhost:8443")
        cfg1.addNamespace("cdntest", "api_key", "cdntest_remote")
        cfg1.addNamespace("game", "game_key", "cdntest_remote")
        cfg1.addLocalLocation(id="stage", loc="<stage>")
        cfg1.addLocalLocation(id="cache", loc=temp_dir)
        
        cat = core.AssetCatalog(space=cfgspc)
        cat.cache_dir = os.path.join(temp_dir, "assetcache")
    
        # Create first manifest
        m1 = cat.createManifest(
            id="cdntest",
            version="1.0.0",
            namespace="cdntest",
            file = core.Path(temp_dir) / "catalog.json")
        
        # Add assets to first manifest
        a1 = m1.createAsset(
            id="user_id",
            priority=100,
            type="text",
            remote = "<cdntest_remote>/my_remote_asset_dir",
            local = temp_dir,
            filename="my_asset_file.txt",
            platforms=["mac", "linux"],
            dependencies = ["fqid1", "fqid2"]
        )
        
        # Create second manifest in different namespace
        m2 = cat.createManifest(
            id="game_assets",
            version="2.0.0",
            namespace="game",
            file = core.Path(temp_dir) / "game_catalog.json")
        
        # Add asset to second manifest
        a2 = m2.createAsset(
            id="player_model",
            priority=50,
            type="model",
            remote = "<cdntest_remote>/models",
            local = temp_dir,
            filename="models/player.gltf",
            platforms=["mac", "linux", "win32"],
            dependencies = []
        )
        
        # Test AssetCatalog.toJson()
        print("=== AssetCatalog.toJson() ===")
        catalog_json = cat.toJson()
        print(catalog_json)
        
        # Parse and verify JSON structure
        parsed = json.loads(catalog_json)
        
        # Should have two manifests
        assert len(parsed) == 2, f"Expected 2 manifests, got {len(parsed)}"
        
        # Check for both namespaces
        assert "cdntest_manifest" in parsed, "Expected 'cdntest_manifest' in catalog"
        assert "game_manifest" in parsed, "Expected 'game_manifest' in catalog"
        
        # Verify cdntest manifest
        cdntest_manifest = parsed["cdntest_manifest"]
        assert cdntest_manifest["namespace"] == "cdntest", f"Expected namespace='cdntest'"
        assert cdntest_manifest["version"] == "1.0.0", f"Expected version='1.0.0'"
        assert "user_id" in cdntest_manifest["assets"], "Expected 'user_id' asset in cdntest manifest"
        
        # Verify game manifest
        game_manifest = parsed["game_manifest"]
        assert game_manifest["namespace"] == "game", f"Expected namespace='game'"
        assert game_manifest["version"] == "2.0.0", f"Expected version='2.0.0'"
        assert "player_model" in game_manifest["assets"], "Expected 'player_model' asset in game manifest"
        
        print("\n✓ AssetCatalog.toJson() works correctly!")
        
        print("\n✅ SUCCESS: AssetCatalog.toJson() test passed!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()