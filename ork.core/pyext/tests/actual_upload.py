#!/usr/bin/env ork.python
"""
Test actual file upload to CDN using UploadManager
"""

import sys
import os
import tempfile
from pathlib import Path

# Add the pyext directory to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

try:
    import orkengine.core as ork
except ImportError as e:
    print(f"Failed to import orkengine.core: {e}")
    sys.exit(1)

# Initialize core - required for many orkid subsystems
ork.coreappinit()

def test_actual_upload():
    """Test uploading a real file to the CDN"""
    print("=== Testing Actual File Upload ===")
    
    # Create a test file in cdntest directory
    from ork import path as ork_path
    
    # Create test subdirectory for uploads
    upload_test_dir = ork_path.cdntest / "upload_tests"
    upload_test_dir.mkdir(parents=True, exist_ok=True)
    
    test_file = upload_test_dir / "test_upload.txt"
    test_content = "This is a test file for upload functionality\n"
    test_file.write_text(test_content)
    print(f"Created test file: {test_file}")
    
    # Create upload manager
    upload_manager = ork.UploadManager()
    print("Created UploadManager")
    
    # Create destination URL
    # Note: CDN must be running on localhost:8443 for this to work
    dest_url = ork.URL("https://localhost:8443/upload/test_upload.txt")
    print(f"Destination URL: {dest_url.to_string()}")
    
    # Create upload
    upload = upload_manager.upload(ork.Path(str(test_file)), dest_url)
    print(f"Created upload, state: {upload.state}")
    
    # Set API key
    upload.api_key = "test_key_12345"  # Should match CDN's valid keys
    upload.ignore_tls_errors = True  # For self-signed cert
    
    # Wait for upload to complete
    import time
    timeout = 10  # seconds
    start_time = time.time()
    
    while upload.state in [ork.UploadState.PENDING, ork.UploadState.UPLOADING]:
        if time.time() - start_time > timeout:
            print("Upload timed out!")
            break
        time.sleep(0.1)
        # Print progress
        progress = upload.getProgress()
        bytes_up = upload.bytes_uploaded
        total = upload.total_bytes
        if total > 0:
            print(f"Progress: {bytes_up}/{total} bytes ({progress*100:.1f}%)")
    
    # Check final state
    final_state = upload.state
    print(f"Final upload state: {final_state}")
    
    if final_state == ork.UploadState.COMPLETED:
        print("✓ Upload successful!")
        return True
    else:
        error_msg = upload.error_message
        print(f"✗ Upload failed: {error_msg}")
        return False

if __name__ == "__main__":
    success = test_actual_upload()
    sys.exit(0 if success else 1)