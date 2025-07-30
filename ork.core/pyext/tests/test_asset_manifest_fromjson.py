#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
import json

ddir = ork_path.data/"cdntest"

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test AssetManifest.fromJson()
    ##################################
    
    print("=== Testing AssetManifest.fromJson() ===\n")
    
    # Create a sample manifest JSON with multiple assets
    manifest_json = {
        "id": "test_manifest",
        "uuid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
        "namespace": "test_namespace",
        "version": "2.1.0",
        "assets": {
            "player_model": {
                "type": "model",
                "priority": 100,
                "remote": "https://cdn.example.com/models",
                "local": "/tmp/models",
                "filename": "player.gltf",
                "platforms": ["mac", "linux", "win32"],
                "dependencies": ["player_texture", "player_skeleton"],
                "content_hash": "model_content_hash",
                "storage_hash": "model_storage_hash",
                "hash_algorithm": "xxhash64",
                "native_size": 2097152,
                "compressed_size": 1048576
            },
            "player_texture": {
                "type": "texture",
                "priority": 90,
                "remote": "https://cdn.example.com/textures",
                "local": "/tmp/textures",
                "filename": "player_diffuse.png",
                "platforms": ["mac", "linux", "win32"],
                "dependencies": [],
                "content_hash": "texture_content_hash",
                "storage_hash": "texture_storage_hash",
                "hash_algorithm": "md5",
                "native_size": 4194304,
                "compressed_size": 2097152
            },
            "game_config": {
                "type": "json",
                "priority": 200,
                "remote": "https://cdn.example.com/configs",
                "local": "/tmp/configs",
                "filename": "settings.json",
                "platforms": ["mac", "linux"],
                "dependencies": [],
                "content_hash": "config_content_hash",
                "storage_hash": "config_storage_hash",
                "hash_algorithm": "sha256",
                "native_size": 8192
            }
        }
    }
    
    json_str = json.dumps(manifest_json, indent=2)
    print("Input JSON:")
    print(json_str[:500] + "...\n")  # Print first 500 chars
    
    # Test fromJson
    manifest = core.AssetManifest.fromJson(json_str)
    
    if manifest is None:
        print("❌ ERROR: fromJson returned None")
    else:
        print("✓ AssetManifest created successfully!")
        
        # Verify manifest fields
        assert manifest.namespace == "test_namespace", f"Expected namespace='test_namespace', got '{manifest.namespace}'"
        assert manifest.version == "2.1.0", f"Expected version='2.1.0', got '{manifest.version}'"
        
        # Verify assets
        assets = manifest.assets
        assert len(assets) == 3, f"Expected 3 assets, got {len(assets)}"
        
        # Check that all assets are present
        assert "player_model" in assets, "Expected 'player_model' in assets"
        assert "player_texture" in assets, "Expected 'player_texture' in assets"
        assert "game_config" in assets, "Expected 'game_config' in assets"
        
        # Verify player_model asset
        player_model = assets["player_model"]
        assert player_model.id == "player_model", f"Expected id='player_model', got '{player_model.id}'"
        assert player_model.type == "model", f"Expected type='model', got '{player_model.type}'"
        assert player_model.priority == 100, f"Expected priority=100, got {player_model.priority}"
        assert player_model.filename == "player.gltf", f"Expected filename='player.gltf', got '{player_model.filename}'"
        assert player_model.size == 2097152, f"Expected size=2097152, got {player_model.size}"
        
        # Verify player_texture asset
        player_texture = assets["player_texture"]
        assert player_texture.type == "texture", f"Expected type='texture', got '{player_texture.type}'"
        assert player_texture.priority == 90, f"Expected priority=90, got {player_texture.priority}"
        assert player_texture.filename == "player_diffuse.png", f"Expected filename='player_diffuse.png'"
        
        # Verify game_config asset
        game_config = assets["game_config"]
        assert game_config.type == "json", f"Expected type='json', got '{game_config.type}'"
        assert game_config.priority == 200, f"Expected priority=200, got {game_config.priority}"
        assert game_config.platforms == ["mac", "linux"], f"Expected platforms=['mac', 'linux'], got {game_config.platforms}"
        
        print("✓ All manifest fields verified!")
        
        # Test round-trip (toJson -> fromJson)
        print("\n=== Round-trip test ===")
        output_json = manifest.toJson()
        print("Output JSON (first 500 chars):")
        print(output_json[:500] + "...\n")
        
        # Parse output and verify structure
        parsed_output = json.loads(output_json)
        assert parsed_output["namespace"] == "test_namespace"
        assert parsed_output["version"] == "2.1.0"
        assert len(parsed_output["assets"]) == 3
        
        # Create manifest from output
        manifest2 = core.AssetManifest.fromJson(output_json)
        assert manifest2 is not None, "Round-trip manifest creation failed"
        assert manifest2.namespace == manifest.namespace
        assert manifest2.version == manifest.version
        assert len(manifest2.assets) == len(manifest.assets)
        
        print("✓ Round-trip successful!")
        
        print("\n✅ SUCCESS: AssetManifest.fromJson() test passed!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()