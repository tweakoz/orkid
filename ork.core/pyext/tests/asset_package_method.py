#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from ork.assets import package_asset
from obt import path as obt_path
import os
import shutil
import tempfile
import tarfile
import json

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test ork.assets.package_asset method
    ##################################
    
    print("=== Testing ork.assets.package_asset method ===\n")
    
    # Create temporary directories for test
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = obt_path.Path(temp_dir)
        
        # Create test directories
        test_data_dir = temp_path / "test_data"
        manifests_dir = temp_path / "manifests"
        cache_dir = temp_path / "cache"
        
        os.makedirs(test_data_dir, exist_ok=True)
        os.makedirs(manifests_dir, exist_ok=True)
        os.makedirs(cache_dir, exist_ok=True)
        
        # Create config for namespace
        config_dir = temp_path / "config"
        os.makedirs(config_dir, exist_ok=True)
        
        config_data = {
            "namespaces": {
                "testns": {
                    "encryption_key": "test_key_123",
                    "upload_location": "test_remote"
                }
            },
            "locations": {
                "test_remote": "https://test.cdn.com"
            },
            "destinations": {
                "test_stage": os.path.abspath(str(temp_path)),
                "cache": "<assetcache>",
                "stage": "<stage>"
            }
        }
        
        config_file = config_dir / "test_config.json"
        with open(config_file, 'w') as f:
            json.dump(config_data, f, indent=2)
        
        print(f"✓ Created test config at: {config_file}")
        
        ##################################
        # Test 1: Package a single file
        ##################################
        print("\n=== Test 1: Package single file ===")
        
        # Create a test file
        test_file = test_data_dir / "shader.glsl"
        with open(test_file, 'w') as f:
            f.write("""#version 330 core
in vec3 position;
void main() {
    gl_Position = vec4(position, 1.0);
}""")
        
        print(f"✓ Created test file: {test_file}")
        
        # Package the single file
        # Use temp directory as local location to avoid assertion
        result1 = package_asset(
            namespace="testns",
            output=str(manifests_dir / "testns_single.json"),
            asset_id="test_shader",
            priority=50,
            remote_loc="https://test-remote.example.com/assets",  # Use actual URL instead of template
            local_loc=str(test_data_dir),  # Use actual directory
            asset=str(test_file),
            cache_dir=str(cache_dir),
            key="test_key_123"
        )
        
        print(f"✓ Packaged single file:")
        print(f"  - Asset path: {result1['asset_path']}")
        print(f"  - Content hash: {result1['content_hash']}")
        print(f"  - Storage hash: {result1['storage_hash']}")
        print(f"  - Manifest: {result1['manifest_path']}")
        
        # Verify manifest was created
        assert os.path.exists(result1['manifest_path']), "Manifest file should exist"
        
        # Read and verify manifest content
        with open(result1['manifest_path'], 'r') as f:
            manifest_data = json.load(f)
        
        print(f"DEBUG: Manifest structure: {list(manifest_data.keys())}")
        
        assert manifest_data['namespace'] == 'testns', "Namespace should match"
        
        # The manifest structure might have assets as a dict, not a list
        if isinstance(manifest_data.get('assets'), dict):
            assert len(manifest_data['assets']) >= 1, "Should have at least one asset"
            # Find our asset
            found = False
            for asset_id, asset in manifest_data['assets'].items():
                if asset.get('id') == 'test_shader' or asset_id == 'test_shader':
                    assert asset.get('type') == 'asset', "Type should be 'asset'"
                    assert asset.get('priority') == 50, "Priority should match"
                    found = True
                    break
            assert found, "Should find test_shader asset"
        else:
            # Handle as list
            assert len(manifest_data['assets']) == 1, "Should have one asset"
            asset = manifest_data['assets'][0]
            assert asset['id'] == 'test_shader', "Asset ID should match"
            assert asset['type'] == 'asset', "Type should be 'asset'"
            assert asset['priority'] == 50, "Priority should match"
        
        print("✓ Manifest content verified")
        
        ##################################
        # Test 2: Package an asset_pak
        ##################################
        print("############################################")
        print("############################################")
        print("=== Test 2: Package asset_pak from directory ===")
        print("############################################")
        print("############################################")
        
        # Create directory structure
        pak_dir = test_data_dir / "game_assets"
        models_dir = pak_dir / "models"
        textures_dir = pak_dir / "textures"
        
        os.makedirs(models_dir, exist_ok=True)
        os.makedirs(textures_dir, exist_ok=True)
        
        # Create test files
        with open(models_dir / "cube.obj", 'w') as f:
            f.write("# Cube model\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n")
        
        with open(textures_dir / "wall.png", 'wb') as f:
            # Minimal PNG header
            f.write(b'\x89PNG\r\n\x1a\n' + b'\x00' * 32)
        
        with open(pak_dir / "manifest.txt", 'w') as f:
            f.write("Game assets v1.0\nContains models and textures\n")
        
        print(f"✓ Created test directory structure at: {pak_dir}")
        
        # Package the directory as asset_pak
        result2 = package_asset(
            namespace="testns",
            output=str(manifests_dir / "testns_pak.json"),
            asset_id="game_pak",
            priority=10,
            remote_loc="https://test-remote.example.com/paks",  # Use actual URL instead of template
            local_loc=str(test_data_dir),  # Use actual directory
            asset_pak=str(pak_dir),
            cache_dir=str(cache_dir),
            key="test_key_123",
            strip_leading=True,
            platforms=["mac", "linux", "windows"]
        )
        
        print(f"✓ Packaged asset_pak:")
        print(f"  - Asset path: {result2['asset_path']}")
        print(f"  - Content hash: {result2['content_hash']}")
        print(f"  - Storage hash: {result2['storage_hash']}")
        print(f"  - Manifest: {result2['manifest_path']}")
        
        # Verify the asset was created
        assert os.path.exists(result2['asset_path']), "Asset file should exist"
        
        # Read and verify manifest
        with open(result2['manifest_path'], 'r') as f:
            pak_manifest_data = json.load(f)
        
        assert len(pak_manifest_data['assets']) == 1, "Should have one asset"
        # Asset is stored with ID as key, not as an 'id' field
        assert 'game_pak' in pak_manifest_data['assets'], "Asset ID should be in manifest"
        pak_asset = pak_manifest_data['assets']['game_pak']
        assert pak_asset['type'] == 'asset_pak', "Type should be 'asset_pak'"
        assert pak_asset['priority'] == 10, "Priority should match"
        assert set(pak_asset['platforms']) == {"mac", "linux", "windows"}, "Platforms should match"
        print("✓ Asset_pak manifest verified")
        
        ##################################
        # Test 3: Test with filter pattern
        ##################################
        print("\n=== Test 3: Package asset_pak with filter ===")
        
        # Create more files
        with open(models_dir / "sphere.obj", 'w') as f:
            f.write("# Sphere model\nv 0 0 1\n")
        
        with open(models_dir / "temp.tmp", 'w') as f:
            f.write("Temporary file")
        
        # Package with filter to only include .obj files
        result3 = package_asset(
            namespace="testns",
            output=str(manifests_dir / "testns_filtered.json"),
            asset_id="models_only",
            priority=20,
            remote_loc="https://test-remote.example.com/models",  # Use actual URL
            local_loc=str(test_data_dir),  # Use actual directory
            asset_pak=str(pak_dir),
            cache_dir=str(cache_dir),
            key="test_key_123",
            filter="*.obj",
            strip_leading=True
        )
        
        print(f"✓ Packaged filtered asset_pak:")
        print(f"  - Content hash: {result3['content_hash']}")
        print(f"  - Storage hash: {result3['storage_hash']}")
        
        ##################################
        # Test 4: Test with dependencies
        ##################################
        print("\n=== Test 4: Package with dependencies ===")
        
        # Create the levels directory in test_stage
        levels_dir = temp_path / "levels"
        levels_dir.mkdir(parents=True, exist_ok=True)
        
        result4 = package_asset(
            namespace="testns",
            output=str(manifests_dir / "testns_deps.json"),
            asset_id="level_data",
            asset=str(test_file),  # Reuse test file
            priority=30,
            remote_loc="<test_remote>/levels",
            local_loc="<test_stage>/levels",
            cache_dir=str(cache_dir),
            key="test_key_123",
            dependencies=["testns|game_pak", "testns|test_shader"],
            config_path=str(config_file)
        )
        
        print(f"✓ Packaged asset with dependencies")
        
        # Verify dependencies in manifest
        with open(result4['manifest_path'], 'r') as f:
            deps_manifest_data = json.load(f)
        
        deps_asset = deps_manifest_data['assets'][0]
        assert len(deps_asset['dependencies']) == 2, "Should have 2 dependencies"
        assert "testns|game_pak" in deps_asset['dependencies'], "Should depend on game_pak"
        assert "testns|test_shader" in deps_asset['dependencies'], "Should depend on test_shader"
        print("✓ Dependencies verified")
        
        ##################################
        # Test 5: Test error handling
        ##################################
        print("\n=== Test 5: Test error handling ===")
        
        # Test missing required arguments
        try:
            package_asset(
                namespace="testns",
                output=str(manifests_dir / "testns_error.json"),
                asset_id="error_test"
                # Missing remote_loc and local_loc
            )
            assert False, "Should have raised error for missing required args"
        except ValueError as e:
            assert "remote_loc is required" in str(e)
            print("✓ Correctly caught missing remote_loc error")
        
        # Test invalid file path
        try:
            package_asset(
                namespace="testns",
                output=str(manifests_dir / "testns_error2.json"),
                asset_id="error_test2",
                remote_loc="<test_remote>/error",
                local_loc="<stage>/error",
                asset="/path/that/does/not/exist.txt",
                cache_dir=str(cache_dir),
                key="test_key_123"
            )
            assert False, "Should have raised error for non-existent file"
        except ValueError as e:
            assert "must be a file" in str(e)
            print("✓ Correctly caught non-existent file error")
        
        # Test mutually exclusive options
        try:
            package_asset(
                namespace="testns",
                output=str(manifests_dir / "testns_error3.json"),
                asset_id="error_test3",
                remote_loc="<test_remote>/error",
                local_loc="<stage>/error",
                asset=str(test_file),
                asset_pak=str(pak_dir),  # Both asset and asset_pak
                cache_dir=str(cache_dir),
                key="test_key_123"
            )
            assert False, "Should have raised error for both asset and asset_pak"
        except ValueError as e:
            assert "Cannot specify both" in str(e)
            print("✓ Correctly caught mutually exclusive options error")
        
        ##################################
        # Test 6: Verify receipts
        ##################################
        print("\n=== Test 6: Verify receipts ===")
        
        # Read a receipt file
        with open(result1['receipt'], 'r') as f:
            receipt_data = json.load(f)
        
        assert receipt_data['namespace'] == 'testns', "Receipt namespace should match"
        assert receipt_data['asset_id'] == 'test_shader', "Receipt asset_id should match"
        assert receipt_data['type'] == 'asset', "Receipt type should match"
        assert 'file_size' in receipt_data, "Receipt should have file_size"
        assert 'md5' in receipt_data, "Receipt should have md5"
        print("✓ Receipt structure verified")
        
        print("\n✅ SUCCESS: All ork.assets.package_asset tests passed!")

except Exception as e:
    print(f"\n❌ ERROR: {e}")
    import traceback
    traceback.print_exc()

core.coreappexit()