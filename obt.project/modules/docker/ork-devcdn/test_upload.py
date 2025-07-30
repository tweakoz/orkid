#!/usr/bin/env python3
"""
Test script for CDN upload functionality
"""

import requests
import hashlib
import sys
import os

def test_upload(api_key, file_path, remote_path, cdn_url="https://localhost:8443"):
    """
    Upload a file to the CDN
    
    Args:
        api_key: API key for authentication
        file_path: Local file to upload
        remote_path: Path on CDN (e.g., "namespace/asset_name.pak")
        cdn_url: CDN base URL
    """
    
    # Read file data
    try:
        with open(file_path, 'rb') as f:
            file_data = f.read()
    except FileNotFoundError:
        print(f"Error: File {file_path} not found")
        return False
    
    # Calculate MD5 for verification
    md5_hash = hashlib.md5(file_data).hexdigest()
    
    # Prepare request
    upload_url = f"{cdn_url}/upload/{remote_path}"
    headers = {
        'X-API-Key': api_key,
        'Content-Type': 'application/octet-stream'
    }
    
    print(f"Uploading {file_path} to {upload_url}")
    print(f"File size: {len(file_data)} bytes")
    print(f"Local MD5: {md5_hash}")
    
    try:
        # Upload file using PUT
        response = requests.put(
            upload_url,
            data=file_data,
            headers=headers,
            verify=False  # Skip SSL verification for self-signed certs
        )
        
        if response.status_code == 201:
            result = response.json()
            print("Upload successful!")
            print(f"Remote path: {result.get('path')}")
            print(f"Remote size: {result.get('size')} bytes")
            print(f"Remote MD5: {result.get('md5')}")
            
            # Verify MD5 matches
            if result.get('md5') == md5_hash:
                print("✓ MD5 hash verification passed")
                return True
            else:
                print("✗ MD5 hash verification failed")
                return False
        else:
            print(f"Upload failed: {response.status_code}")
            print(f"Response: {response.text}")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")
        return False

def test_download(api_key, remote_path, cdn_url="https://localhost:8443"):
    """
    Download a file from the CDN to verify it was uploaded correctly
    """
    download_url = f"{cdn_url}/download/{remote_path}"
    headers = {
        'X-API-Key': api_key
    }
    
    try:
        response = requests.get(
            download_url,
            headers=headers,
            verify=False
        )
        
        if response.status_code == 200:
            print(f"Download successful! Size: {len(response.content)} bytes")
            return response.content
        else:
            print(f"Download failed: {response.status_code}")
            return None
            
    except requests.exceptions.RequestException as e:
        print(f"Download failed: {e}")
        return None

def main():
    if len(sys.argv) < 4:
        print("Usage: python test_upload.py <api_key> <local_file> <remote_path>")
        print("Example: python test_upload.py your_api_key test.pak game/assets/test.pak")
        sys.exit(1)
    
    api_key = sys.argv[1]
    local_file = sys.argv[2] 
    remote_path = sys.argv[3]
    
    # Test upload
    print("=== Testing Upload ===")
    upload_success = test_upload(api_key, local_file, remote_path)
    
    if upload_success:
        print("\n=== Testing Download ===")
        downloaded_data = test_download(api_key, remote_path)
        
        if downloaded_data:
            # Compare with original
            with open(local_file, 'rb') as f:
                original_data = f.read()
            
            if downloaded_data == original_data:
                print("✓ Round-trip verification passed")
            else:
                print("✗ Round-trip verification failed")

if __name__ == "__main__":
    main()