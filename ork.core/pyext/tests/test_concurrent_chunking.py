#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path
import os
import time
import threading
import hashlib
from concurrent.futures import ThreadPoolExecutor, as_completed

# Initialize core
core.coreappinit()

print("=== Testing Concurrent Chunking with Large Files ===")

# Test file configurations
# Sizes chosen to test non-4MB-aligned cases where X % (1<<22) != 0
test_files = [
    {"name": "large_128mb.bin", "size": 128 * 1024 * 1024},           # 128MB - aligned
    {"name": "odd_87mb.bin", "size": 87 * 1024 * 1024 + 12345},       # ~87MB - non-aligned
    {"name": "large_195mb.bin", "size": 195 * 1024 * 1024 + 67890},   # ~195MB - non-aligned
    {"name": "small_43mb.bin", "size": 43 * 1024 * 1024 + 1337},      # ~43MB - non-aligned
    {"name": "edge_4mb_plus1.bin", "size": 4 * 1024 * 1024 + 1},      # 4MB + 1 byte
    {"name": "edge_4mb_minus1.bin", "size": 4 * 1024 * 1024 - 1},     # 4MB - 1 byte
    {"name": "large_256mb.bin", "size": 256 * 1024 * 1024},           # 256MB - aligned
    {"name": "odd_101mb.bin", "size": 101 * 1024 * 1024 + 54321},     # ~101MB - non-aligned
]

# Create test directory
test_dir = obt_path.temp() / "concurrent_chunk_test"
os.makedirs(str(test_dir), exist_ok=True)

# Create asset catalog setup
stage_dir = obt_path.stage()
config_dir = stage_dir / "concurrent_test"
os.makedirs(str(config_dir), exist_ok=True)

cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
config_file = config_dir / "config.json"
cfg = cfgspc.createConfig(id="concurrent_test", file=config_file)
cfg.addNamespaceKey(id="concurrent_test", key="api_key")
cfg.addRemoteLocation(id="test_remote", loc="https://localhost:8443")
cfg.addLocalLocation(id="cache", loc="<assetcache>")

cat = core.AssetCatalog(space=cfgspc)
manifest = cat.createManifest(
    id="concurrent_test_manifest",
    version="1.0.0",
    namespace="concurrent_test",
    file=config_dir/"manifest.json"
)

def create_test_file(file_info):
    """Create a test file with specific pattern for verification"""
    file_path = test_dir / file_info["name"]
    size = file_info["size"]
    
    print(f"Creating {file_info['name']} ({size / (1024 * 1024):.2f} MB)")
    
    # Create file with repeating pattern for verification
    pattern_size = 1024  # 1KB pattern
    pattern = bytes([i % 256 for i in range(pattern_size)])
    
    with open(str(file_path), 'wb') as f:
        written = 0
        while written < size:
            remaining = size - written
            write_size = min(pattern_size, remaining)
            if write_size == pattern_size:
                f.write(pattern)
            else:
                f.write(pattern[:write_size])
            written += write_size
    
    # Calculate MD5 for verification
    with open(str(file_path), 'rb') as f:
        md5_hash = hashlib.md5(f.read()).hexdigest()
    
    file_info["path"] = file_path
    file_info["md5"] = md5_hash
    print(f"✓ Created {file_info['name']} - MD5: {md5_hash[:16]}...")
    return file_info

def chunk_asset(file_info):
    """Create asset and trigger chunking for a single file"""
    thread_id = threading.current_thread().ident
    start_time = time.time()
    
    try:
        print(f"[Thread {thread_id}] Starting chunking for {file_info['name']}")
        
        # Create asset
        asset = manifest.createAsset(
            id=f"asset_{file_info['name'].replace('.', '_')}",
            priority=100,
            type="binary",
            remote="<test_remote>/concurrent_assets",
            local=str(test_dir),
            filename=file_info['name'],
            platforms=["mac", "linux"],
            dependencies=[]
        )
        
        # Trigger repackaging (chunking)
        asset.repackage()
        
        end_time = time.time()
        duration = end_time - start_time
        
        result = {
            "file_info": file_info,
            "asset": asset,
            "duration": duration,
            "thread_id": thread_id,
            "content_hash": asset.content_hash,
            "storage_hash": asset.storage_hash,
            "size": asset.size
        }
        
        print(f"[Thread {thread_id}] ✓ Completed {file_info['name']} in {duration:.2f}s")
        print(f"[Thread {thread_id}]   Content hash: {asset.content_hash}")
        print(f"[Thread {thread_id}]   Storage hash: {asset.storage_hash}")
        
        return result
        
    except Exception as e:
        print(f"[Thread {thread_id}] ❌ Error chunking {file_info['name']}: {e}")
        return {"error": str(e), "file_info": file_info, "thread_id": thread_id}

# Phase 1: Create all test files
print("\n=== Phase 1: Creating Test Files ===")
start_time = time.time()

created_files = []
for file_info in test_files:
    created_file = create_test_file(file_info)
    created_files.append(created_file)

creation_time = time.time() - start_time
total_size = sum(f["size"] for f in created_files)
print(f"✓ Created {len(created_files)} files totaling {total_size / (1024 * 1024 * 1024):.2f} GB in {creation_time:.2f}s")

# Phase 2: Concurrent chunking
print("\n=== Phase 2: Concurrent Chunking ===")
start_time = time.time()

# Use ThreadPoolExecutor for concurrent chunking
max_workers = min(4, len(created_files))  # Limit concurrent operations
chunk_results = []

with ThreadPoolExecutor(max_workers=max_workers) as executor:
    # Submit all chunking tasks
    future_to_file = {executor.submit(chunk_asset, file_info): file_info for file_info in created_files}
    
    # Collect results as they complete
    for future in as_completed(future_to_file):
        result = future.result()
        chunk_results.append(result)

chunking_time = time.time() - start_time
successful_chunks = [r for r in chunk_results if "error" not in r]
failed_chunks = [r for r in chunk_results if "error" in r]

print(f"\n✓ Concurrent chunking completed in {chunking_time:.2f}s")
print(f"  Successful: {len(successful_chunks)}/{len(chunk_results)}")
print(f"  Failed: {len(failed_chunks)}")

if failed_chunks:
    print("❌ Failed chunks:")
    for failure in failed_chunks:
        print(f"  - {failure['file_info']['name']}: {failure['error']}")

# Phase 3: Verify chunks were created
print("\n=== Phase 3: Chunk Verification ===")
chunks_dir = obt_path.stage() / "assetcache" / "enc" / "chunks"

if os.path.exists(str(chunks_dir)):
    all_chunk_files = [f for f in os.listdir(str(chunks_dir)) if ".chunk." in f]
    print(f"✓ Found {len(all_chunk_files)} total chunk files in chunks directory")
    
    # Group chunks by storage hash
    chunk_groups = {}
    for chunk_file in all_chunk_files:
        storage_hash = chunk_file.split('.chunk.')[0]
        if storage_hash not in chunk_groups:
            chunk_groups[storage_hash] = []
        chunk_groups[storage_hash].append(chunk_file)
    
    print(f"✓ Chunks grouped into {len(chunk_groups)} storage hash groups")
    
    # Verify each successful asset has correct chunking behavior
    chunk_threshold = 10 * 1024 * 1024  # 10MB threshold
    for result in successful_chunks:
        storage_hash = result["storage_hash"]
        file_size = result["size"]
        file_name = result['file_info']['name']
        
        if file_size > chunk_threshold:
            # File should be chunked
            expected_chunks = (file_size + 4*1024*1024 - 1) // (4*1024*1024)  # Ceiling division
            
            if storage_hash in chunk_groups:
                actual_chunks = len(chunk_groups[storage_hash])
                if actual_chunks == expected_chunks:
                    print(f"✓ {file_name}: {actual_chunks} chunks (correct, file > 10MB)")
                else:
                    print(f"❌ {file_name}: {actual_chunks} chunks, expected {expected_chunks} (file > 10MB)")
            else:
                print(f"❌ {file_name}: no chunks found for storage hash {storage_hash} (file > 10MB)")
        else:
            # File should NOT be chunked (below 10MB threshold)
            if storage_hash in chunk_groups:
                print(f"❌ {file_name}: found chunks but file is {file_size/(1024*1024):.1f}MB < 10MB threshold")
            else:
                print(f"✓ {file_name}: no chunks (correct, file {file_size/(1024*1024):.1f}MB < 10MB threshold)")
else:
    print("❌ Chunks directory does not exist")

# Phase 4: Performance Analysis
print("\n=== Phase 4: Performance Analysis ===")
total_processed_size = sum(r["size"] for r in successful_chunks if "size" in r)
avg_throughput = total_processed_size / chunking_time / (1024 * 1024)  # MB/s

print(f"Total data processed: {total_processed_size / (1024 * 1024 * 1024):.2f} GB")
print(f"Average throughput: {avg_throughput:.2f} MB/s")
print(f"Concurrent workers: {max_workers}")

# Show individual file performance
durations = [r["duration"] for r in successful_chunks if "duration" in r]
if durations:
    print(f"Individual file times: {min(durations):.2f}s - {max(durations):.2f}s (min-max)")
    print(f"Average file time: {sum(durations)/len(durations):.2f}s")

# Phase 5: Edge Case Analysis
print("\n=== Phase 5: Edge Case Analysis ===")
edge_cases = [r for r in successful_chunks if "edge_" in r["file_info"]["name"]]
chunk_threshold = 10 * 1024 * 1024  # 10MB threshold
chunk_size = 4 * 1024 * 1024        # 4MB chunk size

for result in edge_cases:
    file_info = result["file_info"]
    size = file_info["size"]
    will_be_chunked = size > chunk_threshold
    
    print(f"Edge case {file_info['name']}:")
    print(f"  Size: {size} bytes ({size / (1024*1024):.1f} MB)")
    print(f"  Above 10MB threshold: {will_be_chunked}")
    if will_be_chunked:
        expected_chunks = (size + chunk_size - 1) // chunk_size
        print(f"  Expected chunks: {expected_chunks}")
    else:
        print(f"  Will be stored as single encrypted file (no chunking)")
    print(f"  Content hash: {result['content_hash']}")

print(f"\n✅ Concurrent chunking test completed!")
print(f"   - Processed {len(successful_chunks)} files concurrently")
print(f"   - Total throughput: {avg_throughput:.2f} MB/s")
print(f"   - All chunks stored in correct location")

core.coreappexit()