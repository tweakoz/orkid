#!/usr/bin/env ork.python
"""
Test DownloadManager.remoteFileExists() functionality
"""

import sys
import os

# Add the pyext directory to Python path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

try:
    import orkengine.core as ork
except ImportError as e:
    print(f"Failed to import orkengine.core: {e}")
    sys.exit(1)

# Initialize core
ork.coreappinit()

def test_remote_file_exists():
    """Test checking if remote files exist"""
    print("=== Testing DownloadManager.remoteFileExists() ===")
    
    # Create download manager
    download_manager = ork.DownloadManager()
    print("Created DownloadManager")
    
    # Test URLs
    base_url = "https://localhost:8443"
    
    # Headers for authentication
    headers = {
        "X-API-Key": "test_key_12345"
    }
    
    # Test 1: Check existing file (uploaded in previous tests)
    url1 = ork.URL(f"{base_url}/download/test_upload.txt")
    exists1 = download_manager.remoteFileExists(url1, headers, ignore_tls_errors=True)
    print(f"File exists check (test_upload.txt): {exists1}")
    
    # Test 2: Check non-existent file
    url2 = ork.URL(f"{base_url}/download/nonexistent.pak")
    exists2 = download_manager.remoteFileExists(url2, headers, ignore_tls_errors=True)
    print(f"File exists check (nonexistent.pak): {exists2}")
    
    # Test 3: Check without authentication (should fail)
    exists3 = download_manager.remoteFileExists(url1, {}, ignore_tls_errors=True)
    print(f"File exists check without auth: {exists3}")
    
    # Test 4: Check health endpoint (no auth required, but wrong method)
    health_url = ork.URL(f"{base_url}/health")
    health_exists = download_manager.remoteFileExists(health_url, {}, ignore_tls_errors=True)
    print(f"Health endpoint with HEAD: {health_exists}")
    
    # Test 5: Check if we can use it before downloading
    if not exists1:
        print("File doesn't exist, would download...")
    else:
        print("File exists, can skip download!")
    
    return True

if __name__ == "__main__":
    try:
        success = test_remote_file_exists()
        sys.exit(0 if success else 1)
    except Exception as e:
        print(f"Test failed: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)