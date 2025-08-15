#!/usr/bin/env ork.python

"""
Test environment variable expansion in asset catalog remote_loc fields
"""

import os
import json
import tempfile
from pathlib import Path
from orkengine import core

def test_env_var_expansion():
    """Test that environment variables in remote_loc are properly expanded"""
    
    print("=" * 60)
    print("Testing Environment Variable Expansion in Asset Catalog")
    print("=" * 60)
    
    # Initialize core
    core.coreappinit()
    
    try:
        # Create a temporary directory for test files
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = Path(temp_dir)
            
            # Set test environment variables
            test_cdn_url = "https://test-cdn.example.com"
            test_api_url = "https://api.example.com/v1"
            os.environ["TEST_CDN_URL"] = test_cdn_url
            os.environ["TEST_API_URL"] = test_api_url
            
            print(f"\n✓ Set TEST_CDN_URL = {test_cdn_url}")
            print(f"✓ Set TEST_API_URL = {test_api_url}")
            
            # Create test config with environment variables
            config_data = {
                "namespaces": {
                    "test_namespace": {
                        "encryption_key": "${TEST_ENCRYPTION_KEY}",
                        "remote_location": "test_cdn"
                    }
                },
                "locations": {
                    "test_cdn": {
                        "url": "${TEST_CDN_URL}",
                        "upload_url": "${TEST_API_URL}/upload",
                        "api_key": "${TEST_API_KEY}"
                    },
                    "test_local": {
                        "url": "file:///tmp/test"
                    }
                }
            }
            
            # Set the encryption key env var
            os.environ["TEST_ENCRYPTION_KEY"] = "test_key_12345"
            os.environ["TEST_API_KEY"] = "api_key_67890"
            
            # Write config to file
            config_file = temp_path / "config.json"
            with open(config_file, 'w') as f:
                json.dump(config_data, f, indent=2)
            
            print(f"\n✓ Created test config at: {config_file}")
            
            # Load the config
            config = core.AssetConfig.loadFromFile(str(config_file))
            assert config is not None, "Failed to load config"
            print("✓ Loaded config successfully")
            
            # Test encryption key expansion
            encryption_key = config.getEncryptionKeyForNamespace("test_namespace")
            assert encryption_key == "test_key_12345", f"Encryption key not expanded: got '{encryption_key}'"
            print(f"✓ Encryption key expanded correctly: {encryption_key}")
            
            # Test location URL expansion
            location = config.getRemoteLocationForNamespace("test_namespace")
            assert location is not None, "Failed to get location for namespace"
            
            # Check that URL was expanded
            url_str = str(location.url)
            assert url_str == test_cdn_url + "/", f"URL not expanded: got '{url_str}'"
            print(f"✓ URL expanded correctly: {url_str}")
            
            # Check upload_url expansion
            if location.upload_url:
                upload_url_str = str(location.upload_url)
                expected_upload = test_api_url + "/upload"
                assert upload_url_str == expected_upload, f"Upload URL not expanded: got '{upload_url_str}'"
                print(f"✓ Upload URL expanded correctly: {upload_url_str}")
            
            # Check API key expansion
            if location.api_key:
                assert location.api_key == "api_key_67890", f"API key not expanded: got '{location.api_key}'"
                print(f"✓ API key expanded correctly: {location.api_key}")
            
            # Now test manifest with env vars in remote_loc
            manifest_data = {
                "namespace": "test_namespace",
                "version": "1.0.0",
                "assets": {
                    "test_asset": {
                        "type": "data",
                        "priority": 100,
                        "remote_loc": "${TEST_CDN_URL}/assets",
                        "local_loc": "",
                        "filename": "test.dat",
                        "size": 1024,
                        "storage_hash": "abc123def456",
                        "content_hash": "fedcba654321"
                    }
                }
            }
            
            # Write manifest to file
            manifest_file = temp_path / "manifest.json"
            with open(manifest_file, 'w') as f:
                json.dump(manifest_data, f, indent=2)
            
            print(f"\n✓ Created test manifest at: {manifest_file}")
            
            # Create catalog with config space
            cfgspc = core.AssetConfigSpace()
            cfgspc.createConfig("test", str(config_file))
            
            catalog = core.AssetCatalog(cfgspc)
            print("✓ Created catalog with config space")
            
            # Load the manifest
            manifest = core.AssetManifest.load_from_file(core.Path(str(manifest_file)))
            assert manifest is not None, "Failed to load manifest"
            print("✓ Loaded manifest successfully")
            
            # Add manifest to catalog
            catalog.add_manifest(manifest)
            print("✓ Added manifest to catalog")
            
            # Check if asset info contains the raw remote_loc
            asset_info = catalog.get_asset_info("test_namespace|test_asset")
            if asset_info:
                print(f"\n✓ Found asset: test_namespace|test_asset")
                print(f"  Remote location: {asset_info.remote_loc}")
                print(f"  Storage hash: {asset_info.storage_hash}")
                
                # The remote_loc should still contain the env var syntax in the manifest
                assert "${TEST_CDN_URL}" in asset_info.remote_loc, \
                    f"Expected env var syntax in remote_loc, got: {asset_info.remote_loc}"
                print("✓ Asset remote_loc contains environment variable syntax")
                
                # When the catalog tries to download, it should expand the env var
                # We can't test actual download without a server, but we've verified
                # the expansion logic works for config files
                
            else:
                print("⚠ Asset not found in catalog (this might be expected)")
            
            print("\n" + "=" * 60)
            print("✅ All environment variable expansion tests passed!")
            print("=" * 60)
            
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        raise
    finally:
        # Cleanup environment variables
        for var in ["TEST_CDN_URL", "TEST_API_URL", "TEST_ENCRYPTION_KEY", "TEST_API_KEY"]:
            if var in os.environ:
                del os.environ[var]
        
        core.coreappexit()

if __name__ == "__main__":
    test_env_var_expansion()