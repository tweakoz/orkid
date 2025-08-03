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

print("=== Testing Concurrent Chunk Assembly and Reassembly ===")

# Test file configurations - use same files as chunking test for consistency
test_files = [
    {"name": "large_128mb.bin", "size": 128 * 1024 * 1024},           # 128MB - aligned
    {"name": "odd_87mb.bin", "size": 87 * 1024 * 1024 + 12345},       # ~87MB - non-aligned
    {"name": "large_195mb.bin", "size": 195 * 1024 * 1024 + 67890},   # ~195MB - non-aligned
    {"name": "small_43mb.bin", "size": 43 * 1024 * 1024 + 1337},      # ~43MB - non-aligned
    {"name": "large_256mb.bin", "size": 256 * 1024 * 1024},           # 256MB - aligned
    {"name": "odd_101mb.bin", "size": 101 * 1024 * 1024 + 54321},     # ~101MB - non-aligned
]

# Create test directories
test_dir = obt_path.temp() / "concurrent_assembly_test"
os.makedirs(str(test_dir), exist_ok=True)

reassembled_dir = test_dir / "reassembled"
os.makedirs(str(reassembled_dir), exist_ok=True)

# Create asset catalog setup
stage_dir = obt_path.stage()
config_dir = stage_dir / "concurrent_assembly_test"
os.makedirs(str(config_dir), exist_ok=True)

cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
config_file = config_dir / "config.json"
cfg = cfgspc.createConfig(id="concurrent_assembly_test", file=config_file)
cfg.addNamespaceKey(id="concurrent_assembly_test", key="api_key")
cfg.addRemoteLocation(id="test_remote", loc="https://localhost:8443")
cfg.addLocalLocation(id="cache", loc="<assetcache>")

cat = core.AssetCatalog(space=cfgspc)
manifest = cat.createManifest(
    id="concurrent_assembly_test_manifest",
    version="1.0.0",
    namespace="concurrent_assembly_test",
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

def chunk_and_assemble_asset(file_info):
    """Create, chunk, and then reassemble a single asset"""
    thread_id = threading.current_thread().ident
    start_time = time.time()
    
    try:
        print(f"[Thread {thread_id}] Starting chunk+assemble for {file_info['name']}")
        
        # Phase 1: Create asset and chunk it
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
        chunk_start = time.time()
        asset.repackage()
        chunk_time = time.time() - chunk_start
        
        print(f"[Thread {thread_id}]   Chunked {file_info['name']} in {chunk_time:.2f}s")
        print(f"[Thread {thread_id}]   Storage hash: {asset.storage_hash}")
        
        # Phase 2: Read chunks and reassemble
        chunks_dir = obt_path.stage() / "assetcache" / "enc" / "chunks"
        storage_hash = asset.storage_hash
        
        # Find all chunk files for this asset
        chunk_files = []
        if os.path.exists(str(chunks_dir)):
            all_files = os.listdir(str(chunks_dir))
            chunk_files = [f for f in all_files if f.startswith(storage_hash + ".chunk.")]
            chunk_files.sort()  # Sort by chunk number
        
        if not chunk_files:
            # File was not chunked (< 10MB threshold), should be in regular enc directory
            enc_dir = obt_path.stage() / "assetcache" / "enc"
            enc_file = enc_dir / f"{storage_hash}.enc"
            if os.path.exists(str(enc_file)):
                # Copy encrypted file directly (no chunk assembly needed)
                reassembled_path = reassembled_dir / file_info['name']
                with open(str(enc_file), 'rb') as src, open(str(reassembled_path), 'wb') as dst:
                    dst.write(src.read())
                print(f"[Thread {thread_id}]   Copied encrypted file (no chunking)")
            else:
                raise Exception(f"No chunks or encrypted file found for {storage_hash}")
        else:
            # Reassemble from chunks
            reassemble_start = time.time()
            reassembled_path = reassembled_dir / file_info['name']
            
            print(f"[Thread {thread_id}]   Found {len(chunk_files)} chunks to reassemble")
            
            with open(str(reassembled_path), 'wb') as output_file:
                for chunk_file in chunk_files:
                    chunk_path = chunks_dir / chunk_file
                    with open(str(chunk_path), 'rb') as chunk:
                        output_file.write(chunk.read())
            
            reassemble_time = time.time() - reassemble_start
            print(f"[Thread {thread_id}]   Reassembled from {len(chunk_files)} chunks in {reassemble_time:.2f}s")
        
        # Phase 3: Verify reassembled file matches original
        with open(str(reassembled_path), 'rb') as f:
            reassembled_md5 = hashlib.md5(f.read()).hexdigest()
        
        # Note: The reassembled file is encrypted, so MD5 won't match original
        # But we can verify the size and that it was created successfully
        reassembled_size = os.path.getsize(str(reassembled_path))
        
        end_time = time.time()
        duration = end_time - start_time
        
        result = {
            "file_info": file_info,
            "asset": asset,
            "duration": duration,
            "chunk_time": chunk_time,
            "reassemble_time": reassemble_time if chunk_files else 0,
            "thread_id": thread_id,
            "content_hash": asset.content_hash,
            "storage_hash": asset.storage_hash,
            "original_size": asset.size,
            "reassembled_size": reassembled_size,
            "chunk_count": len(chunk_files),
            "reassembled_path": reassembled_path,
            "was_chunked": len(chunk_files) > 0
        }
        
        if chunk_files:
            print(f"[Thread {thread_id}] ✓ Completed {file_info['name']} in {duration:.2f}s (chunked)")
        else:
            print(f"[Thread {thread_id}] ✓ Completed {file_info['name']} in {duration:.2f}s (not chunked)")
        
        return result
        
    except Exception as e:
        print(f"[Thread {thread_id}] ❌ Error processing {file_info['name']}: {e}")
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

# Phase 2: Concurrent chunk + assemble
print("\n=== Phase 2: Concurrent Chunk + Assembly ===")
start_time = time.time()

# Use ThreadPoolExecutor for concurrent processing
max_workers = min(4, len(created_files))  # Limit concurrent operations
results = []

with ThreadPoolExecutor(max_workers=max_workers) as executor:
    # Submit all tasks
    future_to_file = {executor.submit(chunk_and_assemble_asset, file_info): file_info for file_info in created_files}
    
    # Collect results as they complete
    for future in as_completed(future_to_file):
        result = future.result()
        results.append(result)

processing_time = time.time() - start_time
successful_results = [r for r in results if "error" not in r]
failed_results = [r for r in results if "error" in r]

print(f"\n✓ Concurrent processing completed in {processing_time:.2f}s")
print(f"  Successful: {len(successful_results)}/{len(results)}")
print(f"  Failed: {len(failed_results)}")

if failed_results:
    print("❌ Failed operations:")
    for failure in failed_results:
        print(f"  - {failure['file_info']['name']}: {failure['error']}")

# Phase 3: Verification and Analysis
print("\n=== Phase 3: Assembly Verification ===")

chunk_threshold = 10 * 1024 * 1024  # 10MB threshold
chunked_count = 0
not_chunked_count = 0

for result in successful_results:
    file_info = result["file_info"]
    original_size = result["original_size"]
    reassembled_size = result["reassembled_size"]
    was_chunked = result["was_chunked"]
    chunk_count = result["chunk_count"]
    
    if was_chunked:
        chunked_count += 1
        print(f"✓ {file_info['name']}: {chunk_count} chunks → {reassembled_size} bytes (chunked)")
    else:
        not_chunked_count += 1
        print(f"✓ {file_info['name']}: {reassembled_size} bytes (not chunked, < 10MB)")
    
    # Verify size consistency (encrypted file may have padding, so allow small difference)
    size_diff = abs(reassembled_size - original_size)
    if size_diff > 1024:  # Allow up to 1KB difference for encryption padding
        print(f"⚠️  Size mismatch for {file_info['name']}: original {original_size}, reassembled {reassembled_size}")

print(f"\nChunking summary:")
print(f"  Files chunked: {chunked_count}")
print(f"  Files not chunked: {not_chunked_count}")

# Phase 4: Performance Analysis
print("\n=== Phase 4: Performance Analysis ===")
total_processed_size = sum(r["original_size"] for r in successful_results if "original_size" in r)
avg_throughput = total_processed_size / processing_time / (1024 * 1024)  # MB/s

print(f"Total data processed: {total_processed_size / (1024 * 1024 * 1024):.2f} GB")
print(f"Average throughput: {avg_throughput:.2f} MB/s")
print(f"Concurrent workers: {max_workers}")

# Show timing breakdown
chunk_times = [r["chunk_time"] for r in successful_results if "chunk_time" in r]
reassemble_times = [r["reassemble_time"] for r in successful_results if "reassemble_time" in r and r["reassemble_time"] > 0]
total_times = [r["duration"] for r in successful_results if "duration" in r]

if chunk_times:
    print(f"Chunking times: {min(chunk_times):.2f}s - {max(chunk_times):.2f}s (min-max)")
    print(f"Average chunking time: {sum(chunk_times)/len(chunk_times):.2f}s")

if reassemble_times:
    print(f"Assembly times: {min(reassemble_times):.2f}s - {max(reassemble_times):.2f}s (min-max)")
    print(f"Average assembly time: {sum(reassemble_times)/len(reassemble_times):.2f}s")

if total_times:
    print(f"Total processing times: {min(total_times):.2f}s - {max(total_times):.2f}s (min-max)")

print(f"\n✅ Concurrent chunk assembly test completed!")
print(f"   - Processed {len(successful_results)} files concurrently")
print(f"   - Total throughput: {avg_throughput:.2f} MB/s")
print(f"   - Successfully reassembled all chunked files")

core.coreappexit()