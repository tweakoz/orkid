#!/usr/bin/env ork.python

import tempfile
import os
from orkengine.core import *

##############################################
# Initialize crypto
##############################################

print("Testing crypto initialization...")
assert initialize_crypto() == True
print("✓ Crypto initialized successfully")

##############################################
# Test SecureBuffer creation
##############################################

print("\nTesting SecureBuffer creation...")

# Test size constructor
buf1 = SecureBuffer(32)
assert buf1.size() == 32
print("✓ SecureBuffer with size 32 created")

# Test password constructor
buf2 = SecureBuffer.from_password("test_password", "test_salt")
assert buf2.size() == 32  # ChaCha20 key size
print("✓ SecureBuffer from password created")

##############################################
# Test basic encryption/decryption
##############################################

print("\nTesting basic encryption/decryption...")

# Create codec
codec = create_codec("test_password", "test_namespace")

# Create test data
test_string = b"Hello, this is a test message!"
input_data = DataBlock()
input_data.writeRawData(test_string)

# Encrypt
encrypted = input_data.encrypt(codec)
assert encrypted is not None
assert encrypted.size > input_data.size  # Should be larger due to nonce + tag
print(f"✓ Encrypted {input_data.size} bytes to {encrypted.size} bytes")

# Decrypt
decrypted = encrypted.decrypt(codec)
assert decrypted is not None
assert decrypted.size == input_data.size

# Verify content
decrypted_bytes = bytes(decrypted.bytes)
assert test_string == decrypted_bytes
print("✓ Decryption successful, content matches")

##############################################
# Test chunk encryption/decryption
##############################################

print("\nTesting chunk encryption/decryption...")

codec = LibsodiumCodec("chunk_password", "chunk_context")

# Create test data
test_string = b"Chunk test data that will be encrypted"
input_data = DataBlock()
input_data.writeRawData(test_string)

# Test multiple chunks
for chunk_idx in range(5):
    # Encrypt chunk
    encrypted = codec.encrypt_chunk(input_data, chunk_idx)
    assert encrypted is not None
    
    # Decrypt chunk
    decrypted = codec.decrypt_chunk(encrypted, chunk_idx)
    assert decrypted is not None
    assert decrypted.size == input_data.size
    
    # Verify content
    decrypted_bytes = bytes(decrypted.bytes)
    assert test_string == decrypted_bytes

print("✓ Chunk encryption/decryption successful for 5 chunks")

##############################################
# Test wrong chunk index
##############################################

print("\nTesting wrong chunk index...")

codec = LibsodiumCodec("chunk_password", "chunk_context")

# Create test data
test_string = b"Wrong chunk index test"
input_data = DataBlock()
input_data.writeRawData(test_string)

# Encrypt with chunk index 5
encrypted = codec.encrypt_chunk(input_data, 5)

# Try to decrypt with wrong chunk index
try:
    decrypted = codec.decrypt_chunk(encrypted, 3)  # Wrong index
    assert False, "Should have thrown exception"
except RuntimeError as e:
    assert "Nonce mismatch" in str(e)
    print("✓ Correctly rejected wrong chunk index")

##############################################
# Test wrong password
##############################################

print("\nTesting wrong password...")

# Create test data
test_string = b"Authentication test"
input_data = DataBlock()
input_data.writeRawData(test_string)

# Encrypt with one password
codec1 = create_codec("correct_password", "test_namespace")
encrypted = input_data.encrypt(codec1)

# Try to decrypt with different password
codec2 = create_codec("wrong_password", "test_namespace")
try:
    decrypted = encrypted.decrypt(codec2)
    assert False, "Should have thrown exception"
except RuntimeError as e:
    assert "authentication tag mismatch" in str(e)
    print("✓ Correctly rejected wrong password")

##############################################
# Test large data
##############################################

print("\nTesting large data encryption...")

codec = create_codec("large_data_password", "large_data_context")

# Create large test data (1MB)
import random
random.seed(42)
test_data = bytes([random.randint(0, 255) for _ in range(1024 * 1024)])

input_data = DataBlock()
input_data.writeRawData(test_data)

# Encrypt
encrypted = input_data.encrypt(codec)
assert encrypted is not None
expected_size = len(test_data) + LibsodiumCodec.nonce_size() + LibsodiumCodec.tag_size()
assert encrypted.size == expected_size

# Decrypt
decrypted = encrypted.decrypt(codec)
assert decrypted is not None
assert decrypted.size == len(test_data)

# Verify content
decrypted_bytes = bytes(decrypted.bytes)
assert test_data == decrypted_bytes
print("✓ Successfully encrypted/decrypted 1MB of data")

##############################################
# Test empty data
##############################################

print("\nTesting empty data encryption...")

codec = create_codec("empty_data_password", "empty_data_context")

# Create empty data block
input_data = DataBlock()

# Encrypt empty data
encrypted = input_data.encrypt(codec)
assert encrypted is not None
expected_size = LibsodiumCodec.nonce_size() + LibsodiumCodec.tag_size()
assert encrypted.size == expected_size

# Decrypt
decrypted = encrypted.decrypt(codec)
assert decrypted is not None
assert decrypted.size == 0
print("✓ Successfully encrypted/decrypted empty data")

##############################################
# Test deterministic nonce
##############################################

print("\nTesting deterministic nonce...")

codec = LibsodiumCodec("deterministic_password", "deterministic_context")

# Create test data
test_string = b"Deterministic test"
input_data = DataBlock()
input_data.writeRawData(test_string)

# Encrypt same data with same chunk index multiple times
encrypted1 = codec.encrypt_chunk(input_data, 42)
encrypted2 = codec.encrypt_chunk(input_data, 42)

# Should produce identical ciphertext due to deterministic nonce
assert encrypted1.size == encrypted2.size
assert bytes(encrypted1.bytes) == bytes(encrypted2.bytes)

# Different chunk index should produce different ciphertext
encrypted3 = codec.encrypt_chunk(input_data, 43)
assert bytes(encrypted1.bytes) != bytes(encrypted3.bytes)
print("✓ Deterministic nonce working correctly")

##############################################
# Test codec from key
##############################################

print("\nTesting codec from key...")

# Create a key
key = SecureBuffer(32)

# Create codec from key
codec = create_codec_from_key(key, "key_context")

# Test encryption/decryption
test_string = b"Key-based codec test"
input_data = DataBlock()
input_data.writeRawData(test_string)

encrypted = input_data.encrypt(codec)
assert encrypted is not None

decrypted = encrypted.decrypt(codec)
assert decrypted is not None

decrypted_bytes = bytes(decrypted.bytes)
assert test_string == decrypted_bytes
print("✓ Codec from key working correctly")

##############################################
# Test file operations
##############################################

print("\nTesting file operations...")

codec = create_codec("file_password", "file_context")

# Create temporary files
with tempfile.NamedTemporaryFile(mode='wb', delete=False) as f:
    input_file = f.name
    test_content = b"This is a test file content for encryption"
    f.write(test_content)

encrypted_file = input_file + ".enc"
decrypted_file = input_file + ".dec"

try:
    # Encrypt file
    encrypt_file(input_file, encrypted_file, codec)
    assert os.path.exists(encrypted_file)
    
    # Verify encrypted file is different
    with open(encrypted_file, 'rb') as f:
        encrypted_content = f.read()
    assert test_content != encrypted_content
    
    # Decrypt file
    decrypt_file(encrypted_file, decrypted_file, codec)
    assert os.path.exists(decrypted_file)
    
    # Verify decrypted content matches original
    with open(decrypted_file, 'rb') as f:
        decrypted_content = f.read()
    assert test_content == decrypted_content
    print("✓ File encryption/decryption successful")
    
finally:
    # Cleanup
    for f in [input_file, encrypted_file, decrypted_file]:
        if os.path.exists(f):
            os.unlink(f)

##############################################
# Test codec repr
##############################################

print("\nTesting codec representation...")

codec = create_codec("repr_password", "repr_context")
repr_str = repr(codec)
print(f"Codec repr: {repr_str}")
assert "LibsodiumCodec" in repr_str
print("✓ Codec repr working")

##############################################
# Test algorithm constants
##############################################

print("\nTesting algorithm constants...")

assert LibsodiumCodec.algorithm_name() == "chacha20poly1305"
assert LibsodiumCodec.key_size() == 32
assert LibsodiumCodec.nonce_size() == 12
assert LibsodiumCodec.tag_size() == 16
print("✓ Algorithm constants correct")

##############################################
# Test multiple contexts
##############################################

print("\nTesting multiple contexts...")

password = "same_password"

# Create codecs with different contexts
codec1 = create_codec(password, "context1")
codec2 = create_codec(password, "context2")

# Create test data
test_string = b"Context test data"
input_data = DataBlock()
input_data.writeRawData(test_string)

# Encrypt with first codec
encrypted1 = input_data.encrypt(codec1)

# Try to decrypt with second codec (should fail)
try:
    encrypted1.decrypt(codec2)
    assert False, "Should have thrown exception"
except RuntimeError:
    print("✓ Different contexts correctly produce different keys")

# But decrypting with correct codec should work
decrypted = encrypted1.decrypt(codec1)
assert bytes(decrypted.bytes) == test_string
print("✓ Same context decrypts correctly")

##############################################
# All tests passed!
##############################################

print("\n🎉 All crypto tests passed!")