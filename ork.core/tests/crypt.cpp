////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/util/crypt.h>
#include <ork/kernel/datablock.h>
#include <ork/file/file.h>
#include <ork/kernel/string/ArrayString.h>
#include <random>
#include <cstring>

using namespace ork;
using namespace ork::util::crypt;

///////////////////////////////////////////////////////////////////////////////

TEST(CryptInitialization) {
  bool init_result = initializeCrypto();
  CHECK(init_result);
  
  // Second initialization should also succeed (uses std::once_flag)
  bool init_result2 = initializeCrypto();
  CHECK(init_result2);
}

///////////////////////////////////////////////////////////////////////////////

TEST(SecureBufferConstruction) {
  // Test size constructor
  SecureBuffer buf1(32);
  CHECK_EQUAL(32, buf1.size());
  
  // Test password constructor
  SecureBuffer buf2("my_password", "test_context");
  CHECK_EQUAL(crypto_aead_chacha20poly1305_IETF_KEYBYTES, buf2.size());
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecBasicEncryptDecrypt) {
  // Create codec with password
  auto codec = createCodec("test_password", "test_namespace");
  
  // Create test data
  std::string test_string = "Hello, this is a test message!";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  
  // Encrypt
  auto encrypted = input_data->encrypt(codec);
  CHECK(encrypted != nullptr);
  CHECK(encrypted->length() > input_data->length()); // Should be larger due to nonce + tag
  
  // Decrypt
  auto decrypted = encrypted->decrypt(codec);
  CHECK(decrypted != nullptr);
  CHECK_EQUAL(input_data->length(), decrypted->length());
  
  // Verify content
  std::string decrypted_string(reinterpret_cast<const char*>(decrypted->data()), decrypted->length());
  CHECK_EQUAL(test_string, decrypted_string);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecChunkEncryptDecrypt) {
  auto codec = std::make_shared<LibsodiumCodec>("chunk_password", "chunk_context");
  
  // Create test data
  std::string test_string = "Chunk test data that will be encrypted";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  
  // Test multiple chunks
  for (uint64_t chunk_idx = 0; chunk_idx < 5; ++chunk_idx) {
    // Encrypt chunk
    auto encrypted = codec->encryptChunk(input_data.get(), chunk_idx);
    CHECK(encrypted != nullptr);
    
    // Decrypt chunk
    auto decrypted = codec->decryptChunk(encrypted.get(), chunk_idx);
    CHECK(decrypted != nullptr);
    CHECK_EQUAL(input_data->length(), decrypted->length());
    
    // Verify content
    std::string decrypted_string(reinterpret_cast<const char*>(decrypted->data()), decrypted->length());
    CHECK_EQUAL(test_string, decrypted_string);
  }
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecWrongChunkIndex) {
  auto codec = std::make_shared<LibsodiumCodec>("chunk_password", "chunk_context");
  
  // Create test data
  std::string test_string = "Wrong chunk index test";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  
  // Encrypt with chunk index 5
  auto encrypted = codec->encryptChunk(input_data.get(), 5);
  CHECK(encrypted != nullptr);
  
  // Try to decrypt with wrong chunk index
  bool exception_thrown = false;
  try {
    auto decrypted = codec->decryptChunk(encrypted.get(), 3); // Wrong index
  } catch (const std::runtime_error& e) {
    exception_thrown = true;
    CHECK(std::string(e.what()).find("Nonce mismatch") != std::string::npos);
  }
  CHECK(exception_thrown);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecWrongPassword) {
  // Create test data
  std::string test_string = "Authentication test";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  
  // Encrypt with one password
  auto codec1 = createCodec("correct_password", "test_namespace");
  auto encrypted = input_data->encrypt(codec1);
  CHECK(encrypted != nullptr);
  
  // Try to decrypt with different password
  auto codec2 = createCodec("wrong_password", "test_namespace");
  bool exception_thrown = false;
  try {
    auto decrypted = encrypted->decrypt(codec2);
  } catch (const std::runtime_error& e) {
    exception_thrown = true;
    CHECK(std::string(e.what()).find("authentication tag mismatch") != std::string::npos);
  }
  CHECK(exception_thrown);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecLargeData) {
  auto codec = createCodec("large_data_password", "large_data_context");
  
  // Create large test data (1MB)
  const size_t data_size = 1024 * 1024;
  std::vector<uint8_t> test_data(data_size);
  
  // Fill with pseudo-random data
  std::mt19937 gen(42);
  std::uniform_int_distribution<> dis(0, 255);
  for (size_t i = 0; i < data_size; ++i) {
    test_data[i] = static_cast<uint8_t>(dis(gen));
  }
  
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_data.data(), data_size);
  
  // Encrypt
  auto encrypted = input_data->encrypt(codec);
  CHECK(encrypted != nullptr);
  CHECK(encrypted->length() == data_size + LibsodiumCodec::nonceSize() + LibsodiumCodec::tagSize());
  
  // Decrypt
  auto decrypted = encrypted->decrypt(codec);
  CHECK(decrypted != nullptr);
  CHECK_EQUAL(data_size, decrypted->length());
  
  // Verify content
  CHECK(std::memcmp(test_data.data(), decrypted->data(), data_size) == 0);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecEmptyData) {
  auto codec = createCodec("empty_data_password", "empty_data_context");
  
  // Create empty data block
  auto input_data = std::make_shared<DataBlock>();
  
  // Encrypt empty data
  auto encrypted = input_data->encrypt(codec);
  CHECK(encrypted != nullptr);
  CHECK_EQUAL(LibsodiumCodec::nonceSize() + LibsodiumCodec::tagSize(), encrypted->length());
  
  // Decrypt
  auto decrypted = encrypted->decrypt(codec);
  CHECK(decrypted != nullptr);
  CHECK_EQUAL(0, decrypted->length());
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecDeterministicNonce) {
  auto codec = std::make_shared<LibsodiumCodec>("deterministic_password", "deterministic_context");
  
  // Create test data
  std::string test_string = "Deterministic test";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  
  // Encrypt same data with same chunk index multiple times
  auto encrypted1 = codec->encryptChunk(input_data.get(), 42);
  auto encrypted2 = codec->encryptChunk(input_data.get(), 42);
  
  // Should produce identical ciphertext due to deterministic nonce
  CHECK_EQUAL(encrypted1->length(), encrypted2->length());
  CHECK(std::memcmp(encrypted1->data(), encrypted2->data(), encrypted1->length()) == 0);
  
  // Different chunk index should produce different ciphertext
  auto encrypted3 = codec->encryptChunk(input_data.get(), 43);
  CHECK(std::memcmp(encrypted1->data(), encrypted3->data(), encrypted1->length()) != 0);
}

///////////////////////////////////////////////////////////////////////////////

TEST(SecureBufferFromKey) {
  // Create a key
  SecureBuffer key(32);
  
  // Create codec from key
  auto codec = createCodecFromKey(key, "key_context");
  
  // Test encryption/decryption
  std::string test_string = "Key-based codec test";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  
  auto encrypted = input_data->encrypt(codec);
  CHECK(encrypted != nullptr);
  
  auto decrypted = encrypted->decrypt(codec);
  CHECK(decrypted != nullptr);
  
  std::string decrypted_string(reinterpret_cast<const char*>(decrypted->data()), decrypted->length());
  CHECK_EQUAL(test_string, decrypted_string);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LibsodiumCodecConstants) {
  // Verify algorithm constants
  CHECK_EQUAL("chacha20poly1305", LibsodiumCodec::algorithmName());
  CHECK_EQUAL(crypto_aead_chacha20poly1305_IETF_KEYBYTES, LibsodiumCodec::keySize());
  CHECK_EQUAL(crypto_aead_chacha20poly1305_IETF_NPUBBYTES, LibsodiumCodec::nonceSize());
  CHECK_EQUAL(crypto_aead_chacha20poly1305_IETF_ABYTES, LibsodiumCodec::tagSize());
}

///////////////////////////////////////////////////////////////////////////////