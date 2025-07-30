#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path
import tempfile
import os

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test AssetEntry.repackage()
    ##################################
    
    print("=== Testing AssetEntry.repackage() ===\n")
    
    # Create a temporary test file
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
        test_file_path = f.name
        test_filename = os.path.basename(f.name)
        test_content = "This is test content for asset repackaging!\n" * 100
        f.write(test_content)
    
    print(f"Created test file: {test_file_path}")
    print(f"File size: {os.path.getsize(test_file_path)} bytes")
    
    # Create AssetConfigSpace and AssetCatalog
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    cfg = cfgspc.createConfig(
        id="test_config",
        file=obt_path.stage()/"test_config.json"
    )
    
    # Create catalog and manifest
    cat = core.AssetCatalog(space=cfgspc)
    manifest = cat.createManifest(
        id="test_manifest",
        version="1.0.0",
        namespace="test_namespace",
        file=obt_path.stage()/"test_manifest.json"
    )
    
    # Create asset through manifest
    entry = manifest.createAsset(
        id="test_asset",
        priority=100,
        type="text",
        remote="https://cdn.example.com/assets",
        local=os.path.dirname(test_file_path),
        filename=test_filename,
        platforms=["mac", "linux"],
        dependencies=[]
    )
    
    print(f"\nAssetEntry before repackage:")
    print(f"  filename: {entry.filename}")
    print(f"  local_loc: {entry.local_loc}")
    print(f"  size: {entry.size}")
    print(f"  content_hash: {entry.content_hash}")
    print(f"  storage_hash: {entry.storage_hash}")
    print(f"  is_chunked: {entry.is_chunked()}")
    
    # Call repackage
    print("\nCalling repackage()...")
    entry.repackage()
    
    print(f"\nAssetEntry after repackage:")
    print(f"  filename: {entry.filename}")
    print(f"  local_loc: {entry.local_loc}")
    print(f"  size: {entry.size}")
    print(f"  content_hash: {entry.content_hash}")
    print(f"  storage_hash: {entry.storage_hash}")
    print(f"  hash_algorithm: {entry.hash_algorithm}")
    print(f"  is_chunked: {entry.is_chunked()}")
    
    # Verify results
    assert entry.size == os.path.getsize(test_file_path), "Size should match file size"
    assert entry.content_hash != "", "Content hash should be computed"
    assert entry.storage_hash != "", "Storage hash should be computed"
    assert entry.content_hash != entry.storage_hash, "Storage hash should be different from content hash (simulated encryption)"
    assert entry.hash_algorithm == "xxhash64", "Hash algorithm should be xxhash64"
    assert not entry.is_chunked(), "Small file should not be chunked"
    
    print("\n✅ SUCCESS: Basic repackage test passed!")
    
    # Test with larger file that should be chunked
    print("\n=== Testing chunking for large file ===")
    
    # Create a larger file (>10MB)
    with tempfile.NamedTemporaryFile(mode='wb', suffix='.bin', delete=False) as f:
        large_file_path = f.name
        large_filename = os.path.basename(f.name)
        # Write 11MB of data
        chunk_data = b'X' * (1024 * 1024)  # 1MB chunk
        for i in range(11):
            f.write(chunk_data)
    
    print(f"\nCreated large file: {large_file_path}")
    print(f"File size: {os.path.getsize(large_file_path)} bytes")
    
    # Create entry for large file
    large_entry = manifest.createAsset(
        id="large_asset",
        priority=100,
        type="binary",
        remote="https://cdn.example.com/assets",
        local=os.path.dirname(large_file_path),
        filename=large_filename,
        platforms=["mac", "linux"],
        dependencies=[]
    )
    
    # Repackage large file
    print("\nCalling repackage() on large file...")
    large_entry.repackage()
    
    print(f"\nLarge AssetEntry after repackage:")
    print(f"  size: {large_entry.size}")
    print(f"  content_hash: {large_entry.content_hash}")
    print(f"  is_chunked: {large_entry.is_chunked()}")
    
    assert large_entry.is_chunked(), "Large file should be chunked"
    assert large_entry.size == 11 * 1024 * 1024, "Size should be 11MB"
    
    print("\n✅ SUCCESS: Chunking test passed!")
    
    # Clean up
    os.unlink(test_file_path)
    os.unlink(large_file_path)
    
    print("\n✅ SUCCESS: All AssetEntry.repackage() tests passed!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()
    # Clean up on error
    if 'test_file_path' in locals() and os.path.exists(test_file_path):
        os.unlink(test_file_path)
    if 'large_file_path' in locals() and os.path.exists(large_file_path):
        os.unlink(large_file_path)

core.coreappexit()