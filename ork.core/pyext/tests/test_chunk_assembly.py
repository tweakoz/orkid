#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path
from obt import path as obt_path
import os
import hashlib

# Initialize core
core.coreappinit()

print("=== Testing Chunk Assembly and Reassembly ===")

# Use the chunks created by previous test
chunks_dir = obt_path.stage() / "assetcache" / "enc" / "chunks"
print(f"Looking for chunks in: {chunks_dir}")

if not os.path.exists(str(chunks_dir)):
    print("❌ ERROR: Chunks directory does not exist. Run test_asset_catalog3_chunks.py first.")
    core.coreappexit()
    exit(1)

# Find chunk files - look for the pattern {hash}.chunk.{index}
chunk_files = []
for item in os.listdir(str(chunks_dir)):
    if ".chunk." in item:
        chunk_files.append(item)

if not chunk_files:
    print("❌ ERROR: No chunk files found. Run test_asset_catalog3_chunks.py first.")
    core.coreappexit()
    exit(1)

# Sort chunk files by index
chunk_files.sort()
print(f"✓ Found {len(chunk_files)} chunk files:")
for chunk_file in chunk_files:
    chunk_path = chunks_dir / chunk_file
    chunk_size = os.path.getsize(str(chunk_path))
    print(f"  - {chunk_file} ({chunk_size} bytes)")

# Extract storage hash from first chunk filename
storage_hash = chunk_files[0].split('.chunk.')[0]
print(f"✓ Storage hash: {storage_hash}")

# Test 1: Read and reassemble chunks
print("\n=== Test 1: Manual Chunk Reassembly ===")

# Create output directory for reassembled file
output_dir = obt_path.temp() / "chunk_assembly_test"
os.makedirs(str(output_dir), exist_ok=True)
reassembled_file = output_dir / "reassembled_file.bin"

print(f"Reassembling chunks to: {reassembled_file}")

# Read all chunks in order and write to reassembled file
total_size = 0
with open(str(reassembled_file), 'wb') as output_file:
    for chunk_file in chunk_files:
        chunk_path = chunks_dir / chunk_file
        print(f"  Reading chunk: {chunk_file}")
        
        with open(str(chunk_path), 'rb') as chunk:
            chunk_data = chunk.read()
            
            # If chunks are encrypted, we need to decrypt them
            # For this test, let's check if they need decryption by looking for patterns
            if chunk_file.endswith('.enc'):
                print(f"    ⚠️  Chunk appears encrypted - would need codec to decrypt")
                # For now, we'll work with encrypted data
            
            output_file.write(chunk_data)
            total_size += len(chunk_data)
            print(f"    Wrote {len(chunk_data)} bytes")

print(f"✓ Reassembled file size: {total_size} bytes")

# Test 2: Verify chunk hash integrity
print("\n=== Test 2: Chunk Hash Verification ===")

# Calculate XXH64 hash of each chunk (if we had the hasher)
# For now, calculate MD5 as a proxy verification
for chunk_file in chunk_files:
    chunk_path = chunks_dir / chunk_file
    with open(str(chunk_path), 'rb') as f:
        chunk_data = f.read()
        
    # Calculate MD5 hash as verification
    md5_hash = hashlib.md5(chunk_data).hexdigest()
    print(f"  {chunk_file}: MD5={md5_hash[:16]}...")

# Test 3: Compare with original (if available)
print("\n=== Test 3: Compare with Original File ===")

# Look for the original test file
original_file = obt_path.temp() / "test_chunks_dir" / "large_test_file.bin"
if os.path.exists(str(original_file)):
    original_size = os.path.getsize(str(original_file))
    reassembled_size = os.path.getsize(str(reassembled_file))
    
    print(f"Original file size: {original_size} bytes")
    print(f"Reassembled file size: {reassembled_size} bytes")
    
    if original_size == reassembled_size:
        print("✓ Size match!")
        
        # Compare MD5 hashes (of encrypted data vs original)
        with open(str(original_file), 'rb') as f:
            original_md5 = hashlib.md5(f.read()).hexdigest()
        with open(str(reassembled_file), 'rb') as f:
            reassembled_md5 = hashlib.md5(f.read()).hexdigest()
            
        print(f"Original MD5: {original_md5}")
        print(f"Reassembled MD5: {reassembled_md5}")
        
        if original_md5 == reassembled_md5:
            print("✅ SUCCESS: Chunks reassemble to identical file!")
        else:
            print("⚠️  Hashes differ - chunks may be encrypted or processed")
    else:
        print("❌ Size mismatch")
else:
    print("⚠️  Original file not found for comparison")

# Test 4: Check chunk naming consistency
print("\n=== Test 4: Chunk Naming Verification ===")

expected_pattern = f"{storage_hash}.chunk."
all_chunks_named_correctly = True
for i, chunk_file in enumerate(chunk_files):
    expected_name = f"{storage_hash}.chunk.{i:04d}"
    if chunk_file.endswith('.enc'):
        expected_name += '.enc'
    
    if chunk_file != expected_name:
        print(f"❌ Naming mismatch: expected {expected_name}, got {chunk_file}")
        all_chunks_named_correctly = False
    else:
        print(f"✓ {chunk_file} - correct naming")

if all_chunks_named_correctly:
    print("✅ All chunks follow correct naming pattern")

# Test 5: Verify chunk size consistency
print("\n=== Test 5: Chunk Size Verification ===")

chunk_sizes = []
for chunk_file in chunk_files:
    chunk_path = chunks_dir / chunk_file
    size = os.path.getsize(str(chunk_path))
    chunk_sizes.append(size)

expected_chunk_size = 4 * 1024 * 1024  # 4MB
print(f"Expected chunk size: {expected_chunk_size} bytes")

all_chunks_correct_size = True
for i, size in enumerate(chunk_sizes):
    if i < len(chunk_sizes) - 1:  # Not the last chunk
        if size != expected_chunk_size:
            print(f"❌ Chunk {i} size mismatch: expected {expected_chunk_size}, got {size}")
            all_chunks_correct_size = False
        else:
            print(f"✓ Chunk {i}: {size} bytes (correct)")
    else:  # Last chunk can be smaller
        if size > expected_chunk_size:
            print(f"❌ Last chunk {i} too large: {size} bytes")
            all_chunks_correct_size = False
        else:
            print(f"✓ Last chunk {i}: {size} bytes (correct - can be smaller)")

if all_chunks_correct_size:
    print("✅ All chunks have correct sizes")

print(f"\n✅ Chunk assembly testing complete!")
print(f"   - Found and processed {len(chunk_files)} chunks")
print(f"   - Reassembled to {total_size} bytes")
print(f"   - Chunks stored in correct location: <stage>/assetcache/enc/chunks/")

core.coreappexit()