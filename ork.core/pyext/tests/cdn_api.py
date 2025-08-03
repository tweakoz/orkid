#!/usr/bin/env ork.python
"""
Test CDN API endpoints and standard HTTP methods
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

# Initialize core
ork.coreappinit()

def test_cdn_api():
    """Test CDN API endpoints"""
    print("=== Testing CDN API Endpoints ===")
    
    # Create HTTPS uploader config
    config = ork.HttpsUploaderConfig()
    config.host = "localhost"
    config.port = 8443
    config.api_key = "test_key_12345"
    config.verify_ssl = False
    
    # Create HTTPS uploader
    uploader = ork.HttpsUploader(config)
    print("Created HttpsUploader")
    
    # Test 1: Check if file exists (should return False for non-existent file)
    exists = uploader.remoteFileExists("/download/nonexistent.pak")
    print(f"File exists check (nonexistent): {exists}")
    
    # Test 2: Check if uploaded file exists
    exists = uploader.remoteFileExists("/download/test_upload.txt")
    print(f"File exists check (test_upload.txt): {exists}")
    
    # Test 3: List remote directory
    files = uploader.listRemoteDirectory("/")
    print(f"Remote files: {files}")
    
    # Test 4: Upload a new test file
    # Use cdntest directory for all CDN-related files
    from ork import path as ork_path
    api_test_dir = ork_path.cdntest / "api_tests"
    api_test_dir.mkdir(parents=True, exist_ok=True)
    
    temp_file = api_test_dir / "api_test.txt"
    temp_file.write_text("Testing CDN API endpoints\n")
    
    success = uploader.uploadFile(ork.Path(str(temp_file)), "/upload/api_test.txt")
    print(f"Upload test file: {'Success' if success else 'Failed'}")
    
    # Test 5: Verify uploaded file exists
    if success:
        exists = uploader.remoteFileExists("/download/api_test.txt")
        print(f"Verify uploaded file exists: {exists}")
    
    # Test 6: List files again to see new file
    files = uploader.listRemoteDirectory("/")
    print(f"Remote files after upload: {files}")
    
    print("\n=== Testing Standard HTTP Methods ===")
    
    # Test HEAD request via curl
    import subprocess
    
    # Test HEAD on existing file
    result = subprocess.run([
        "curl", "-I", "-k", "-H", "X-API-Key: test_key_12345",
        "https://localhost:8443/download/test_upload.txt"
    ], capture_output=True, text=True)
    
    print("HEAD request on existing file:")
    for line in result.stdout.split('\n')[:10]:  # First 10 lines
        if line.strip():
            print(f"  {line}")
    
    # Test API endpoints
    print("\n=== Testing API Endpoints ===")
    
    # Test /api/health (no auth required)
    result = subprocess.run([
        "curl", "-k", "https://localhost:8443/api/health"
    ], capture_output=True, text=True)
    print(f"Health check: {result.stdout}")
    
    # Test /api/list
    result = subprocess.run([
        "curl", "-k", "-H", "X-API-Key: test_key_12345",
        "https://localhost:8443/api/list"
    ], capture_output=True, text=True)
    print(f"List API: {result.stdout[:200]}...")  # First 200 chars
    
    # Test /api/stats
    result = subprocess.run([
        "curl", "-k", "-H", "X-API-Key: test_key_12345",
        "https://localhost:8443/api/stats"
    ], capture_output=True, text=True)
    print(f"Stats API: {result.stdout}")
    
    return True

if __name__ == "__main__":
    try:
        success = test_cdn_api()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Test failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)