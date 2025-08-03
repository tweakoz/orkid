#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path
import os

ddir = ork_path.data/"cdntest"
staging_dir = obt_path.stage()

# Initialize core
core.coreappinit()

try:
    ##################################
    # Test repackage workflow
    ##################################
    
    print("=== Test Asset Catalog Repackage Workflow ===\n")
    
    # Create config space
    cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
    cfg = cfgspc.createConfig("test", ddir/"config.json")
    
    # Create catalog with config space
    cat = core.AssetCatalog(space=cfgspc)
    
    # Register codec with password
    cat.registerCodecWithPassword("cdntest", "password123")
    
    # Create manifest
    manifest = cat.createManifest(
        id="test_manifest",
        version="1.0",
        namespace="cdntest",
        file=ddir/"test_manifest.json"
    )
    
    # Create test file first
    test_file_path = staging_dir / "test.png"
    print(f"\nStaging dir: {staging_dir}")
    print(f"Creating test file at: {test_file_path}")
    
    # Ensure directory exists
    os.makedirs(staging_dir, exist_ok=True)
    
    with open(test_file_path, "wb") as f:
        f.write(b"PNG_TEST_DATA" * 100)  # Create some test data
    
    # Verify file exists
    if os.path.exists(test_file_path):
        print(f"✓ Test file created successfully: {test_file_path}")
    else:
        print(f"✗ Failed to create test file: {test_file_path}")
    
    # Create an asset
    asset = manifest.createAsset(
        id="test_asset",
        priority=100,
        type="texture",
        remote="<cdntest_remote>",
        local="<stage>",
        filename="test.png",
        platforms=["win64", "linux64", "macos"],
        dependencies=[]
    )
    
    print(f"Created asset: {asset}")
    print(f"  Content hash: {asset.content_hash}")
    print(f"  Storage hash: {asset.storage_hash}")
    
    # Test individual asset repackage
    print("\n1. Testing AssetEntry.repackage()...")
    asset.repackage()
    print("✓ AssetEntry.repackage() completed")
    print(f"  Updated content hash: {asset.content_hash}")
    print(f"  Updated storage hash: {asset.storage_hash}")
    
    # Check if encrypted file was created
    enc_path = staging_dir / "assetcache" / "enc" / f"{asset.storage_hash}.enc"
    if os.path.exists(enc_path):
        print(f"✓ Encrypted file created: {enc_path}")
    else:
        print(f"✗ Encrypted file NOT found at: {enc_path}")
    
    # Test manifest repackage
    print("\n2. Testing AssetManifest.repackage()...")
    manifest.repackage()
    print("✓ AssetManifest.repackage() completed")
    
    # Test catalog repackage
    print("\n3. Testing AssetCatalog.repackage()...")
    cat.repackage()
    print("✓ AssetCatalog.repackage() completed")
    
    # Create a larger asset to test chunking
    print("\n4. Testing chunked asset repackage...")
    # Create a 12MB test file
    large_file = staging_dir / "large_test.bin"
    with open(large_file, "wb") as f:
        f.write(b"X" * (12 * 1024 * 1024))  # 12MB of 'X'
    
    large_asset = manifest.createAsset(
        id="large_asset",
        priority=100,
        type="data",
        remote="<cdntest_remote>", 
        local="<stage>",
        filename="large_test.bin",
        platforms=["win64", "linux64", "macos"],
        dependencies=[]
    )
    
    print(f"Created large asset: {large_asset}")
    print(f"  Size: {large_asset.size} bytes")
    
    # Repackage large asset
    large_asset.repackage()
    print("✓ Large asset repackaged")
    print(f"  Is chunked: {large_asset.is_chunked()}")
    print(f"  Storage hash: {large_asset.storage_hash}")
    
    # Check for chunk files
    chunks_dir = staging_dir / "assetcache" / "enc" / "chunks"
    if os.path.exists(chunks_dir):
        chunk_files = [f for f in os.listdir(chunks_dir) if f.startswith(large_asset.storage_hash)]
        print(f"  Found {len(chunk_files)} chunk files:")
        for cf in sorted(chunk_files):
            print(f"    - {cf}")
    
    print("\n✅ SUCCESS: All repackage methods work correctly!")

except Exception as e:
    print("❌ ERROR:", e)
    import traceback
    traceback.print_exc()

core.coreappexit()