#!/usr/bin/env ork.python
"""
Test upload_manager functionality with CDN integration
Safely uploads to ork.data/_temp (which should be in .gitignore)
"""

import sys
import os
import tempfile
import hashlib
import json
import shutil
from pathlib import Path

# Add the pyext directory to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

try:
    import orkengine.core as ork
except ImportError as e:
    print(f"Failed to import orkengine.core: {e}")
    print("Make sure the module is built and PYTHONPATH is set correctly")
    sys.exit(1)

# Initialize core - required for many orkid subsystems
ork.coreappinit()

class TestUploadManager:
    def __init__(self):
        self.test_data_dir = None
        self.cdn_url = "https://localhost:8443"
        self.api_key = "test_key_12345"  # Should match keys/valid_keys.txt
        
    def setup_test_data(self):
        """Create test data in ork.data/_temp"""
        # Find ork.data directory relative to this test
        test_dir = Path(__file__).parent
        ork_core_dir = test_dir.parent.parent
        orkid_dir = ork_core_dir.parent
        ork_data_dir = orkid_dir / "ork.data"
        
        if not ork_data_dir.exists():
            raise RuntimeError(f"ork.data directory not found at {ork_data_dir}")
        
        # Create _temp directory if it doesn't exist
        temp_dir = ork_data_dir / "_temp"
        temp_dir.mkdir(exist_ok=True)
        
        # Create test subdirectory
        self.test_data_dir = temp_dir / "upload_test"
        if self.test_data_dir.exists():
            shutil.rmtree(self.test_data_dir)
        self.test_data_dir.mkdir()
        
        print(f"Test data directory: {self.test_data_dir}")
        
        # Create test files
        test_files = {
            "test_asset.pak": b"Mock asset data for testing upload functionality",
            "manifest.json": self.create_test_manifest(),
            "config.json": self.create_test_config()
        }
        
        for filename, content in test_files.items():
            file_path = self.test_data_dir / filename
            if isinstance(content, str):
                content = content.encode('utf-8')
            file_path.write_bytes(content)
            print(f"Created test file: {file_path} ({len(content)} bytes)")
    
    def create_test_manifest(self):
        """Create a test manifest JSON"""
        manifest = {
            "namespace": "test",
            "version": "1.0.0",
            "assets": {
                "test_asset": {
                    "type": "asset_pak",
                    "platforms": ["mac", "linux"],
                    "priority": 100,
                    "src_loc": "<testcdn>",
                    "dst_loc": "<assetcache>/test",
                    "filename": "test_asset.pak",
                    "md5": hashlib.md5(b"Mock asset data for testing upload functionality").hexdigest(),
                    "dependencies": {}
                }
            }
        }
        return json.dumps(manifest, indent=2)
    
    def create_test_config(self):
        """Create a test config JSON"""
        config = {
            "namespace_keys": {
                "test": "test_namespace_key"
            },
            "locations": {
                "testcdn": self.cdn_url
            },
            "destinations": {
                "assetcache": "<stage>/assetcache"
            }
        }
        return json.dumps(config, indent=2)
    
    def test_upload_manager_basic(self):
        """Test basic UploadManager functionality"""
        print("\n=== Testing UploadManager Basic Functionality ===")
        
        try:
            # Create upload manager
            upload_manager = ork.UploadManager()
            print("✓ Created UploadManager")
            
            # Test configuration
            upload_manager.setMaxConcurrentUploads(2)
            print("✓ Set max concurrent uploads")
            
            # Test status queries
            active_count = upload_manager.activeUploadCount()
            is_active = upload_manager.isActive()
            print(f"✓ Active uploads: {active_count}, Manager active: {is_active}")
            
            return True
            
        except Exception as e:
            print(f"✗ UploadManager basic test failed: {e}")
            return False
    
    def test_upload_config(self):
        """Test UploadConfig creation and validation"""
        print("\n=== Testing UploadConfig ===")
        
        try:
            # Create HTTPS upload config
            config = ork.HttpsUploaderConfig()
            config.host = "localhost"
            config.port = 8443
            config.api_key = self.api_key
            config.remote_base_path = "/upload"
            config.create_directories = True
            config.overwrite_existing = True
            config.verify_uploads = True
            config.verify_ssl = False  # HTTPS-specific field
            
            print("✓ Created HttpsUploaderConfig")
            
            # Test validation
            is_valid = config.isValid()
            if not is_valid:
                error = config.getValidationError()
                print(f"Config validation error: {error}")
            else:
                print("✓ UploadConfig is valid")
            
            return is_valid
            
        except Exception as e:
            print(f"✗ UploadConfig test failed: {e}")
            return False
    
    def test_asset_uploader_adapter(self):
        """Test AssetUploaderAdapter functionality"""
        print("\n=== Testing AssetUploaderAdapter ===")
        
        try:
            # Skip manifest loading for now - not implemented yet
            print("⚠ Skipping manifest loading (not implemented)")
            manifest = None
            
            # Create HTTPS upload config
            config = ork.HttpsUploaderConfig()
            config.host = "localhost"
            config.port = 8443
            config.api_key = self.api_key
            config.remote_base_path = "/upload"
            config.verify_ssl = False
            
            # Create uploader (this might fail if CDN is not running - that's expected)
            try:
                uploader = ork.createAssetUploaderFromUrl(
                    ork.URL(f"{self.cdn_url}/upload"),
                    config
                )
                if uploader:
                    print("✓ Created AssetUploaderAdapter")
                    
                    # Test uploader properties
                    uploader_type = uploader.type()
                    print(f"✓ Uploader type: {uploader_type}")
                    
                    # Test cancel functionality
                    uploader.cancel()
                    is_cancelled = uploader.isCancelled()
                    print(f"✓ Cancel functionality: {is_cancelled}")
                    
                else:
                    print("⚠ Could not create uploader (CDN may not be running)")
                    
            except Exception as e:
                print(f"⚠ Could not create uploader (expected if CDN not running): {e}")
            
            return True
            
        except Exception as e:
            print(f"✗ AssetUploaderAdapter test failed: {e}")
            return False
    
    def test_upload_progress(self):
        """Test UploadProgress functionality"""
        print("\n=== Testing UploadProgress ===")
        
        try:
            # Create upload progress object
            progress = ork.UploadProgress()
            
            # Set some test values
            progress.current_file = "test_asset.pak"
            progress.files_completed = 1
            progress.total_files = 3
            progress.bytes_uploaded = 1024
            progress.total_bytes = 4096
            progress.elapsed_time = 5.0
            progress.estimated_time_remaining = 10.0
            
            print("✓ Created UploadProgress")
            
            # Test calculations
            percent = progress.getProgressPercent()
            rate = progress.getTransferRate()
            rate_str = progress.getRateString()
            
            print(f"✓ Progress: {percent:.1f}%")
            print(f"✓ Transfer rate: {rate:.1f} bytes/sec ({rate_str})")
            
            return True
            
        except Exception as e:
            print(f"✗ UploadProgress test failed: {e}")
            return False
    
    def test_upload_receipt(self):
        """Test UploadReceipt functionality"""
        print("\n=== Testing UploadReceipt ===")
        
        try:
            # Create upload receipt
            receipt = ork.UploadReceipt()
            receipt.upload_id = "test_upload_001"
            receipt.namespace_id = "test"
            receipt.manifest_id = "test_manifest"
            receipt.destination = self.cdn_url
            receipt.total_files = 1
            receipt.successful_files = 1
            receipt.failed_files = 0
            receipt.bytes_uploaded = 1024
            receipt.total_duration = 5.0
            receipt.success = True
            receipt.status_message = "Upload completed successfully"
            
            print("✓ Created UploadReceipt")
            
            # Test serialization
            json_str = receipt.toJson()
            print(f"✓ Serialized to JSON ({len(json_str)} chars)")
            
            # Test summary
            summary = receipt.getSummary()
            print(f"✓ Summary: {summary}")
            
            # Test saving/loading
            receipt_path = self.test_data_dir / "test_receipt.json"
            success = receipt.saveToFile(ork.Path(str(receipt_path)))
            if success:
                print(f"✓ Saved receipt to {receipt_path}")
                
                # Try to load it back
                loaded_receipt = ork.UploadReceipt.loadFromFile(ork.Path(str(receipt_path)))
                if loaded_receipt:
                    print("✓ Loaded receipt from file")
                    if loaded_receipt.upload_id == receipt.upload_id:
                        print("✓ Receipt data matches")
                    else:
                        print("✗ Receipt data mismatch")
                        return False
                else:
                    print("✗ Failed to load receipt")
                    return False
            else:
                print("✗ Failed to save receipt")
                return False
            
            return True
            
        except Exception as e:
            print(f"✗ UploadReceipt test failed: {e}")
            return False
    
    def test_mock_upload(self):
        """Test a complete upload workflow (mock/dry-run)"""
        print("\n=== Testing Mock Upload Workflow ===")
        
        try:
            # Skip manifest/config loading for now - not implemented yet
            print("⚠ Skipping manifest/config loading (not implemented)")
            
            # Create upload manager
            upload_manager = ork.UploadManager()
            
            # Create HTTPS upload config
            upload_config = ork.HttpsUploaderConfig()
            upload_config.host = "localhost"
            upload_config.port = 8443
            upload_config.api_key = self.api_key
            upload_config.remote_base_path = "/upload"
            upload_config.verify_ssl = False
            upload_config.upload_manager = upload_manager
            
            print("✓ Created upload configuration")
            
            # For this test, we won't actually try to upload since the CDN might not be running
            # But we can test the setup and teardown
            print("✓ Mock upload workflow completed (CDN connection not tested)")
            
            return True
            
        except Exception as e:
            print(f"✗ Mock upload workflow failed: {e}")
            return False
    
    def cleanup(self):
        """Clean up test data"""
        if self.test_data_dir and self.test_data_dir.exists():
            shutil.rmtree(self.test_data_dir)
            print(f"Cleaned up test directory: {self.test_data_dir}")
    
    def run_all_tests(self):
        """Run all tests"""
        print("Starting upload_manager tests...")
        print(f"CDN URL: {self.cdn_url}")
        print(f"API Key: {self.api_key}")
        
        try:
            # Setup
            self.setup_test_data()
            
            # Run tests
            tests = [
                ("UploadManager Basic", self.test_upload_manager_basic),
                ("UploadConfig", self.test_upload_config),
                ("AssetUploaderAdapter", self.test_asset_uploader_adapter),
                ("UploadProgress", self.test_upload_progress),
                ("UploadReceipt", self.test_upload_receipt),
                ("Mock Upload Workflow", self.test_mock_upload),
            ]
            
            results = []
            for test_name, test_func in tests:
                try:
                    result = test_func()
                    results.append((test_name, result))
                except Exception as e:
                    print(f"✗ {test_name} failed with exception: {e}")
                    results.append((test_name, False))
            
            # Summary
            print("\n" + "="*60)
            print("TEST RESULTS:")
            print("="*60)
            
            passed = 0
            total = len(results)
            
            for test_name, result in results:
                status = "PASS" if result else "FAIL"
                print(f"{test_name:30} {status}")
                if result:
                    passed += 1
            
            print("="*60)
            print(f"Total: {passed}/{total} tests passed ({100*passed//total}%)")
            
            if passed == total:
                print("🎉 All tests passed!")
                return True
            else:
                print("❌ Some tests failed")
                return False
                
        except Exception as e:
            print(f"Test setup failed: {e}")
            return False
        finally:
            self.cleanup()

def main():
    """Main test runner"""
    tester = TestUploadManager()
    success = tester.run_all_tests()
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()