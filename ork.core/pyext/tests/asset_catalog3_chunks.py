#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path
import os

# Initialize core
core.coreappinit()

print("=== Testing Chunked Assets ===")

# Create a test directory with a large file (>10MB to trigger chunking)
# Use temp directory for temporary test files
test_dir = obt_path.temp() / "test_chunks_dir"
os.makedirs(str(test_dir), exist_ok=True)

# Create a large test file (12MB to exceed 10MB chunk threshold)
large_file_path = test_dir / "large_test_file.bin"
large_file_size = 12 * 1024 * 1024  # 12MB

print(f"✓ Creating large test file: {large_file_path}")
print(f"  Size: {large_file_size / (1024 * 1024):.1f} MB")

# Create the large file with pattern data for verification
with open(str(large_file_path), 'wb') as f:
    # Write pattern data so we can verify chunks later
    pattern = b"CHUNK_TEST_DATA_" + b"A" * 48  # 64 bytes total
    for i in range(large_file_size // len(pattern)):
        f.write(pattern)
    # Write remaining bytes
    remaining = large_file_size % len(pattern)
    if remaining > 0:
        f.write(pattern[:remaining])

print(f"✓ Created test file: {large_file_path}")
print(f"  Actual size: {os.path.getsize(str(large_file_path))} bytes")

# Create asset catalog (use staging dir for config like other tests)
stage_dir = obt_path.stage()
config_dir = stage_dir / "chunkstest"
os.makedirs(str(config_dir), exist_ok=True)

# No need to create config file - createConfig should create new config

cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
config_file = config_dir / "config.json"
cfg = cfgspc.createConfig(id="test_chunks", file=config_file)
cfg.addRemoteLocation(id="test_remote", loc="https://localhost:8443")
cfg.addNamespace("test_chunks", "api_key", "test_remote")
cfg.addLocalLocation(id="cache", loc="<assetcache>")

cat = core.AssetCatalog(space=cfgspc)

# Create manifest
manifest = cat.createManifest(
    id="test_chunks_manifest",
    version="1.0.0",
    namespace="test_chunks",
    file=config_dir/"manifest.json"
)

# Create asset that will be chunked
asset = manifest.createAsset(
    id="large_test_asset",
    priority=100,
    type="binary",
    remote="<test_remote>/large_assets",
    local=str(test_dir),
    filename="large_test_file.bin",
    platforms=["mac", "linux"],
    dependencies=[]
)

print(f"✓ Created asset: {asset.id}")
print(f"  Type: {asset.type}")
print(f"  Local: {asset.local_loc}")
print(f"  Filename: {asset.filename}")

# Trigger repackaging to create chunks
print("\n=== Repackaging Asset (will trigger chunking) ===")
asset.repackage()

print(f"✓ Asset repackaged")
print(f"  Content hash: {asset.content_hash}")
print(f"  Storage hash: {asset.storage_hash}")
print(f"  Size: {asset.size}")

# Check if chunks were created
chunks_dir = obt_path.stage() / "assetcache" / "enc" / "chunks"
print(f"\n=== Checking for chunks in: {chunks_dir} ===")

if os.path.exists(str(chunks_dir)):
    # List all files in chunks directory
    import subprocess
    result = subprocess.run(['ls', '-la', str(chunks_dir)], capture_output=True, text=True)
    print("Chunks directory contents:")
    print(result.stdout)
    
    # Look for chunk files with our storage hash
    chunk_files = []
    for item in os.listdir(str(chunks_dir)):
        if asset.storage_hash in item and ".chunk." in item:
            chunk_files.append(item)
    
    if chunk_files:
        print(f"✓ Found {len(chunk_files)} chunk files:")
        for chunk_file in sorted(chunk_files):
            chunk_path = chunks_dir / chunk_file
            chunk_size = os.path.getsize(str(chunk_path))
            print(f"  - {chunk_file} ({chunk_size} bytes)")
        
        # Verify chunk naming pattern
        expected_pattern = f"{asset.storage_hash}.chunk."
        all_chunks_named_correctly = all(expected_pattern in cf for cf in chunk_files)
        if all_chunks_named_correctly:
            print("✓ All chunks follow correct naming pattern")
        else:
            print("❌ Some chunks have incorrect naming pattern")
        
        print(f"\n✅ SUCCESS: Chunked asset created correctly!")
        print(f"   - Large file ({large_file_size / (1024 * 1024):.1f} MB) was chunked")
        print(f"   - {len(chunk_files)} chunks written to <stage>/assetcache/enc/chunks/")
        print(f"   - Chunks use storage hash: {asset.storage_hash}")
        
    else:
        print(f"❌ ERROR: No chunk files found with storage hash {asset.storage_hash}")
        
else:
    print(f"❌ ERROR: Chunks directory does not exist: {chunks_dir}")

# Check if chunk manifest was created
if hasattr(asset, 'chunk_manifest') and asset.chunk_manifest:
    print(f"\n=== Chunk Manifest ===")
    print(f"  Total size: {asset.chunk_manifest.total_size}")
    print(f"  Chunk size: {asset.chunk_manifest.chunk_size}")
    print(f"  Number of chunks: {len(asset.chunk_manifest.chunks)}")
    print(f"  File hash: {asset.chunk_manifest.file_hash}")
    print(f"  Is encrypted: {asset.chunk_manifest.is_encrypted}")
else:
    print("❌ ERROR: No chunk manifest found")

core.coreappexit()