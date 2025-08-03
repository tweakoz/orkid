#!/usr/bin/env ork.python

import random
from orkengine.core import *

##############################################
# Test basic compression/decompression
##############################################

print("Testing basic LZ4 compression/decompression...")

# Create test data
test_string = b"Hello, this is a test message that will be compressed!"
input_data = DataBlock()
input_data.writeRawData(test_string)

# Compress
compressed = input_data.compressed()
assert compressed is not None
# Note: For small data, compressed might be larger due to header overhead
print(f"✓ Compressed {input_data.size} bytes to {compressed.size} bytes")

# Decompress
decompressed = compressed.decompressed()
assert decompressed is not None
assert decompressed.size == input_data.size

# Verify content
assert bytes(decompressed.bytes) == test_string
print("✓ Decompression successful, content matches")

##############################################
# Test compression levels
##############################################

print("\nTesting compression levels...")

# Create test data with repetitive pattern (compresses well)
input_data = DataBlock()
for i in range(1000):
    input_data.writeRawData(b"ABCDEFGHIJKLMNOPQRSTUVWXYZ")

# Test different compression levels
compressed_fast = input_data.compressed(0)  # Fast compression
compressed_hc1 = input_data.compressed(1)   # HC level 1
compressed_hc9 = input_data.compressed(9)   # HC level 9

assert compressed_fast is not None
assert compressed_hc1 is not None
assert compressed_hc9 is not None

# HC should generally produce smaller output
assert compressed_hc9.size <= compressed_hc1.size
assert compressed_hc9.size <= compressed_fast.size

print(f"✓ Fast: {compressed_fast.size} bytes")
print(f"✓ HC1: {compressed_hc1.size} bytes")
print(f"✓ HC9: {compressed_hc9.size} bytes")

# All should decompress to same data
decompressed_fast = compressed_fast.decompressed()
decompressed_hc1 = compressed_hc1.decompressed()
decompressed_hc9 = compressed_hc9.decompressed()

assert decompressed_fast.size == input_data.size
assert decompressed_hc1.size == input_data.size
assert decompressed_hc9.size == input_data.size

assert bytes(decompressed_fast.bytes) == bytes(input_data.bytes)
assert bytes(decompressed_hc1.bytes) == bytes(input_data.bytes)
assert bytes(decompressed_hc9.bytes) == bytes(input_data.bytes)

print("✓ All compression levels decompress correctly")

##############################################
# Test large data compression
##############################################

print("\nTesting large data compression...")

# Create large test data (1MB)
data_size = 1024 * 1024
input_data = DataBlock()

# Fill with compressible pattern
pattern = b"The quick brown fox jumps over the lazy dog. "
test_data = b""
while len(test_data) < data_size:
    test_data += pattern[:min(len(pattern), data_size - len(test_data))]
input_data.writeRawData(test_data)

# Compress
compressed = input_data.compressed()
assert compressed is not None
assert compressed.size < input_data.size  # Should compress somewhat
compression_ratio = (1.0 - compressed.size / input_data.size) * 100
print(f"✓ Compressed 1MB to {compressed.size} bytes ({compression_ratio:.1f}% reduction)")

# Decompress
decompressed = compressed.decompressed()
assert decompressed is not None
assert decompressed.size == data_size

# Verify content
assert bytes(decompressed.bytes) == test_data
print("✓ Large data decompression successful")

##############################################
# Test empty data compression
##############################################

print("\nTesting empty data compression...")

# Create empty data block
input_data = DataBlock()

# Compress empty data
compressed = input_data.compressed()
assert compressed is not None
assert compressed.size == 12  # Header only (magic + size)

# Decompress should also return empty
decompressed = compressed.decompressed()
assert decompressed is not None
assert decompressed.size == 0
print("✓ Empty data compression/decompression successful")

##############################################
# Test random data (worst case)
##############################################

print("\nTesting random data compression...")

# Create truly random data
data_size = 10000
input_data = DataBlock()

random.seed(12345)
test_data = bytes([random.randint(0, 255) for _ in range(data_size)])
input_data.writeRawData(test_data)

# Compress
compressed = input_data.compressed()
assert compressed is not None
# Random data might not compress much, could even be larger due to header

# Decompress
decompressed = compressed.decompressed()
assert decompressed is not None
assert decompressed.size == data_size

# Verify content
assert bytes(decompressed.bytes) == test_data
print(f"✓ Random data: {input_data.size} -> {compressed.size} bytes (expected minimal compression)")

##############################################
# Test invalid decompression
##############################################

print("\nTesting invalid decompression...")

# Test decompression of non-compressed data
invalid_data = DataBlock()
invalid_data.writeRawData(b"This is not compressed data")

try:
    decompressed = invalid_data.decompressed()
    assert False, "Should have thrown exception"
except RuntimeError as e:
    assert "magic header" in str(e)
    print("✓ Correctly rejected non-compressed data")

##############################################
# Test round trip compression
##############################################

print("\nTesting round trip compression...")

# Test multiple round trips
test_string = b"Round trip compression test data!"
original = DataBlock()
original.writeRawData(test_string)

# Multiple compression/decompression cycles
data = original
for i in range(5):
    data = data.compressed()
    assert data is not None
    data = data.decompressed()
    assert data is not None

# Final data should match original
assert data.size == original.size
assert bytes(data.bytes) == bytes(original.bytes)
print("✓ Multiple round trips successful")

##############################################
# Test compression with different data types
##############################################

print("\nTesting compression with different data types...")

# Test with binary data created through DataBlock methods
input_data = DataBlock()
input_data.writeInt(42)
input_data.writeString("test string")
input_data.writeBytes(b"raw bytes data")

original_size = input_data.size

# Compress and decompress
compressed = input_data.compressed()
decompressed = compressed.decompressed()

assert decompressed.size == original_size
assert bytes(decompressed.bytes) == bytes(input_data.bytes)
print("✓ Mixed data types compression successful")

##############################################
# Test file-like data
##############################################

print("\nTesting file-like data compression...")

# Simulate JSON data (highly compressible)
json_data = b'{"name": "test", "values": [' + b', '.join([b'1' for _ in range(1000)]) + b']}'
input_data = DataBlock()
input_data.writeRawData(json_data)

compressed = input_data.compressed()
compression_ratio = (1.0 - compressed.size / input_data.size) * 100
print(f"✓ JSON data compressed by {compression_ratio:.1f}%")

decompressed = compressed.decompressed()
assert bytes(decompressed.bytes) == json_data

##############################################
# All tests passed!
##############################################

print("\n🎉 All LZ4 compression tests passed!")