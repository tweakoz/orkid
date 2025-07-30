////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/kernel/datablock.h>
#include <random>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4CompressionBasic) {
  // Create test data
  std::string test_string = "Hello, this is a test message that will be compressed!";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  
  // Compress
  auto compressed = input_data->compressed();
  CHECK(compressed != nullptr);
  // Note: For small data, compressed might be larger due to header overhead
  
  // Decompress
  auto decompressed = compressed->decompressed();
  CHECK(decompressed != nullptr);
  CHECK_EQUAL(input_data->length(), decompressed->length());
  
  // Verify content
  CHECK(std::memcmp(input_data->data(), decompressed->data(), input_data->length()) == 0);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4CompressionLevels) {
  // Create test data with repetitive pattern (compresses well)
  auto input_data = std::make_shared<DataBlock>();
  for (int i = 0; i < 1000; ++i) {
    input_data->addData("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26);
  }
  
  // Test different compression levels
  auto compressed_fast = input_data->compressed(0);  // Fast compression
  auto compressed_hc1 = input_data->compressed(1);   // HC level 1
  auto compressed_hc9 = input_data->compressed(9);   // HC level 9
  
  CHECK(compressed_fast != nullptr);
  CHECK(compressed_hc1 != nullptr);
  CHECK(compressed_hc9 != nullptr);
  
  // HC should generally produce smaller output
  CHECK(compressed_hc9->length() <= compressed_hc1->length());
  CHECK(compressed_hc9->length() <= compressed_fast->length());
  
  // All should decompress to same data
  auto decompressed_fast = compressed_fast->decompressed();
  auto decompressed_hc1 = compressed_hc1->decompressed();
  auto decompressed_hc9 = compressed_hc9->decompressed();
  
  CHECK_EQUAL(input_data->length(), decompressed_fast->length());
  CHECK_EQUAL(input_data->length(), decompressed_hc1->length());
  CHECK_EQUAL(input_data->length(), decompressed_hc9->length());
  
  CHECK(std::memcmp(input_data->data(), decompressed_fast->data(), input_data->length()) == 0);
  CHECK(std::memcmp(input_data->data(), decompressed_hc1->data(), input_data->length()) == 0);
  CHECK(std::memcmp(input_data->data(), decompressed_hc9->data(), input_data->length()) == 0);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4CompressionLargeData) {
  // Create large test data (1MB)
  const size_t data_size = 1024 * 1024;
  auto input_data = std::make_shared<DataBlock>();
  
  // Fill with compressible pattern
  input_data->reserve(data_size);
  std::string pattern = "The quick brown fox jumps over the lazy dog. ";
  while (input_data->length() < data_size) {
    input_data->addData(pattern.c_str(), std::min(pattern.length(), data_size - input_data->length()));
  }
  
  // Compress
  auto compressed = input_data->compressed();
  CHECK(compressed != nullptr);
  CHECK(compressed->length() < input_data->length()); // Should compress well
  
  // Decompress
  auto decompressed = compressed->decompressed();
  CHECK(decompressed != nullptr);
  CHECK_EQUAL(data_size, decompressed->length());
  
  // Verify content
  CHECK(std::memcmp(input_data->data(), decompressed->data(), data_size) == 0);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4CompressionEmptyData) {
  // Create empty data block
  auto input_data = std::make_shared<DataBlock>();
  
  // Compress empty data
  auto compressed = input_data->compressed();
  CHECK(compressed != nullptr);
  CHECK_EQUAL(12, compressed->length()); // Header only (magic + size)
  
  // Decompress should also return empty
  auto decompressed = compressed->decompressed();
  CHECK(decompressed != nullptr);
  CHECK_EQUAL(0, decompressed->length());
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4CompressionRandomData) {
  // Create truly random data (worst case for compression)
  const size_t data_size = 10000;
  auto input_data = std::make_shared<DataBlock>();
  
  std::mt19937 gen(12345);
  std::uniform_int_distribution<> dis(0, 255);
  
  input_data->reserve(data_size);
  for (size_t i = 0; i < data_size; ++i) {
    uint8_t byte = static_cast<uint8_t>(dis(gen));
    input_data->addItem<uint8_t>(byte);
  }
  
  // Compress
  auto compressed = input_data->compressed();
  CHECK(compressed != nullptr);
  // Random data might not compress much, could be larger due to header
  
  // Decompress
  auto decompressed = compressed->decompressed();
  CHECK(decompressed != nullptr);
  CHECK_EQUAL(data_size, decompressed->length());
  
  // Verify content
  CHECK(std::memcmp(input_data->data(), decompressed->data(), data_size) == 0);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4DecompressionInvalidData) {
  // Test decompression of non-compressed data
  auto invalid_data = std::make_shared<DataBlock>();
  invalid_data->addData("This is not compressed data", 27);
  
  bool exception_thrown = false;
  try {
    auto decompressed = invalid_data->decompressed();
  } catch (const std::runtime_error& e) {
    exception_thrown = true;
    CHECK(std::string(e.what()).find("magic header") != std::string::npos);
  }
  CHECK(exception_thrown);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4DecompressionCorruptedData) {
  // Create valid compressed data first
  std::string test_string = "Valid data to compress";
  auto input_data = std::make_shared<DataBlock>();
  input_data->addData(test_string.c_str(), test_string.length());
  auto compressed = input_data->compressed();
  
  // Corrupt the magic header - this should always fail
  auto corrupted = compressed->clone();
  if (corrupted->_storage.size() > 4) {
    // Corrupt the magic number
    corrupted->_storage[0] ^= 0xFF;
    corrupted->_storage[1] ^= 0xFF;
  }
  
  bool exception_thrown = false;
  try {
    auto decompressed = corrupted->decompressed();
  } catch (const std::runtime_error& e) {
    exception_thrown = true;
    CHECK(std::string(e.what()).find("magic header") != std::string::npos);
  }
  CHECK(exception_thrown);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4CompressionName) {
  // Test that compression adds .lz4 to name
  auto input_data = std::make_shared<DataBlock>();
  input_data->_name = "test_data";
  input_data->addData("Some test data", 14);
  
  auto compressed = input_data->compressed();
  CHECK_EQUAL("test_data.lz4", compressed->_name);
  
  // Test that decompression removes .lz4 from name
  auto decompressed = compressed->decompressed();
  CHECK_EQUAL("test_data", decompressed->_name);
}

///////////////////////////////////////////////////////////////////////////////

TEST(LZ4CompressionRoundTrip) {
  // Test multiple round trips
  std::string test_string = "Round trip compression test data!";
  auto original = std::make_shared<DataBlock>();
  original->addData(test_string.c_str(), test_string.length());
  
  // Multiple compression/decompression cycles
  auto data = original;
  for (int i = 0; i < 5; ++i) {
    data = data->compressed();
    CHECK(data != nullptr);
    data = data->decompressed();
    CHECK(data != nullptr);
  }
  
  // Final data should match original
  CHECK_EQUAL(original->length(), data->length());
  CHECK(std::memcmp(original->data(), data->data(), original->length()) == 0);
}

///////////////////////////////////////////////////////////////////////////////