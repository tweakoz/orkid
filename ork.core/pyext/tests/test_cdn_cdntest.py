#!/usr/bin/env ork.python
"""
Test CDN functionality with the new cdntest directory
This verifies that the CDN can serve files from obt.path.stage()/cdntest
"""

import sys
import os
from pathlib import Path
import json

# Add the pyext directory to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

try:
    import orkengine.core as ork
    from ork import path as ork_path
except ImportError as e:
    print(f"Failed to import modules: {e}")
    sys.exit(1)

# Initialize core
ork.coreappinit()

def test_cdntest_directory():
    """Test CDN serving files from cdntest directory"""
    print("=== Testing CDN with cdntest Directory ===")
    
    # Get the cdntest directory path
    cdntest_dir = ork_path.cdntest
    print(f"CDN content directory: {cdntest_dir}")
    
    # Check if manifest exists (created by CDN launch)
    manifest_path = cdntest_dir / "manifest.json"
    if manifest_path.exists():
        with open(manifest_path, 'r') as f:
            manifest = json.load(f)
        print(f"\nFound manifest with {manifest['total_files']} files:")
        for file_info in manifest['files']:
            print(f"  - {file_info['path']} ({file_info['size']} bytes)")
    else:
        print("No manifest.json found - CDN may not have been launched yet")
        return False
    
    # Create HTTPS uploader config
    config = ork.HttpsUploaderConfig()
    config.host = "localhost"
    config.port = 8443
    config.api_key = "test_key_12345"
    config.verify_ssl = False
    
    # Create HTTPS uploader
    uploader = ork.HttpsUploader(config)
    print("\nCreated HttpsUploader")
    
    # Test downloading files from the manifest
    print("\n=== Testing File Downloads ===")
    success_count = 0
    
    for file_info in manifest['files'][:3]:  # Test first 3 files
        file_path = file_info['path']
        expected_size = file_info['size']
        
        # Check if file exists on CDN
        exists = uploader.remoteFileExists(f"/download/{file_path}")
        print(f"\nFile: {file_path}")
        print(f"  Expected size: {expected_size} bytes")
        print(f"  Exists on CDN: {exists}")
        
        if exists:
            success_count += 1
    
    # List all files on CDN
    print("\n=== CDN Directory Listing ===")
    files = uploader.listRemoteDirectory("/")
    print(f"Total files on CDN: {len(files)}")
    if files:
        print("Files:")
        for f in sorted(files)[:10]:  # Show first 10
            print(f"  - {f}")
        if len(files) > 10:
            print(f"  ... and {len(files) - 10} more files")
    
    # Test upload to cdntest
    print("\n=== Testing Upload to cdntest ===")
    upload_dir = ork_path.cdntest / "upload_tests"
    upload_dir.mkdir(parents=True, exist_ok=True)
    
    test_file = upload_dir / "cdn_upload_test.txt"
    test_file.write_text("Test upload to cdntest directory\n")
    
    success = uploader.uploadFile(ork.Path(str(test_file)), "/upload/test/cdn_upload_test.txt")
    print(f"Upload test file: {'Success' if success else 'Failed'}")
    
    if success:
        # Verify it exists
        exists = uploader.remoteFileExists("/download/test/cdn_upload_test.txt")
        print(f"Verify uploaded file exists: {exists}")
    
    # Clean up
    test_file.unlink()
    
    return success_count > 0

if __name__ == "__main__":
    try:
        success = test_cdntest_directory()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Test failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)