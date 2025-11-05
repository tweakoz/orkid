////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <ork/util/crypt.h>
#include <ork/kernel/datablock.h>
#include <string>
#include <vector>
#include <cstring>

using namespace ork;
using namespace ork::util::crypt;

////////////////////////////////////////////////////////////////
// Test Functions
////////////////////////////////////////////////////////////////

void runCryptoTests(void) {
    auto logchan = logger()->configureChannel("CRYPTO", fvec3(0.9f, 0.5f, 0.9f), true);

    logchan->log("========================================");
    logchan->log("Starting Crypto Tests");
    logchan->log("ChaCha20-Poly1305 (libsodium)");
    logchan->log("========================================");

    // Initialize libsodium
    if (!initializeCrypto()) {
        logchan->log("✗ Failed to initialize libsodium");
        return;
    }
    logchan->log("✓ libsodium initialized");

    // Test 1: Basic Encryption/Decryption with raw key
    logchan->log("");
    logchan->log("--- Test 1: Basic Encryption/Decryption ---");

    {
        // Generate a test key (32 bytes)
        SecureBuffer test_key(LibsodiumCodec::keySize());
        for (size_t i = 0; i < LibsodiumCodec::keySize(); i++) {
            test_key.data()[i] = (uint8_t)i;
        }

        std::string context = "test_context";
        std::string plaintext = "The quick brown fox jumps over the lazy dog";

        // Create codec
        auto codec = createCodecFromKey(test_key, context);

        // Create input DataBlock
        auto input = std::make_shared<DataBlock>();
        input->addData((const uint8_t*)plaintext.c_str(), plaintext.length());

        // Encrypt
        auto encrypted = codec->encrypt(input.get());

        if (encrypted) {
            logchan->log("✓ Encryption successful");
            logchan->log("  Plaintext:  %zu bytes", input->length());
            logchan->log("  Encrypted:  %zu bytes", encrypted->length());
            logchan->log("  Overhead:   %zu bytes (nonce + tag)",
                        encrypted->length() - input->length());

            // Decrypt
            auto decrypted = codec->decrypt(encrypted.get());

            if (decrypted) {
                logchan->log("✓ Decryption successful");
                logchan->log("  Decrypted:  %zu bytes", decrypted->length());

                // Verify
                if (decrypted->length() == input->length() &&
                    memcmp(decrypted->data(), input->data(), input->length()) == 0) {
                    logchan->log("✓ Round-trip verification PASSED");

                    // Display recovered text
                    std::string recovered((const char*)decrypted->data(), decrypted->length());
                    logchan->log("  Recovered: '%s'", recovered.c_str());
                } else {
                    logchan->log("✗ Round-trip verification FAILED");
                }
            } else {
                logchan->log("✗ Decryption failed");
            }
        } else {
            logchan->log("✗ Encryption failed");
        }
    }

    // Test 2: Large Data Encryption (8MB)
    logchan->log("");
    logchan->log("--- Test 2: Large Data Encryption ---");

    {
        SecureBuffer test_key(LibsodiumCodec::keySize());
        for (size_t i = 0; i < LibsodiumCodec::keySize(); i++) {
            test_key.data()[i] = (uint8_t)(i ^ 0x55);
        }

        std::string context = "large_data_test";
        auto codec = createCodecFromKey(test_key, context);

        // Generate 8MB of test data
        size_t data_size = 8 * 1024 * 1024;
        auto input = std::make_shared<DataBlock>();
        input->reserve(data_size);

        uint8_t* data = (uint8_t*)input->allocateBlock(data_size);
        for (size_t i = 0; i < data_size; i++) {
            data[i] = (uint8_t)((i * 131 + 17) & 0xFF);
        }

        logchan->log("Generated %.2f MB of test data", data_size / (1024.0 * 1024.0));

        // Encrypt
        auto encrypted = codec->encrypt(input.get());

        if (encrypted) {
            logchan->log("✓ Encrypted %.2f MB -> %.2f MB",
                        input->length() / (1024.0 * 1024.0),
                        encrypted->length() / (1024.0 * 1024.0));

            // Decrypt

            auto decrypted = codec->decrypt(encrypted.get());

            if (decrypted && decrypted->length() == input->length() &&
                memcmp(decrypted->data(), input->data(), input->length()) == 0) {
                logchan->log("✓ Large data round-trip PASSED");
            } else {
                logchan->log("✗ Large data round-trip FAILED");
            }

        } else {
            logchan->log("✗ Large data encryption failed");
        }
    }

    // Test 3: Chunk-based Encryption (deterministic nonces)
    logchan->log("");
    logchan->log("--- Test 3: Chunk-based Encryption ---");

    {
        SecureBuffer test_key(LibsodiumCodec::keySize());
        for (size_t i = 0; i < LibsodiumCodec::keySize(); i++) {
            test_key.data()[i] = (uint8_t)(i ^ 0xAA);
        }

        std::string context = "chunk_test";
        auto codec = createCodecFromKey(test_key, context);

        // Create 3 chunks
        const int num_chunks = 3;
        std::vector<datablock_ptr_t> chunks;

        for (int i = 0; i < num_chunks; i++) {
            auto chunk = std::make_shared<DataBlock>();
            std::string data = "Chunk " + std::to_string(i) + " data content";
            chunk->addData((const uint8_t*)data.c_str(), data.length());
            chunks.push_back(chunk);
        }

        // Encrypt each chunk with different index
        std::vector<datablock_ptr_t> encrypted_chunks;
        for (int i = 0; i < num_chunks; i++) {
            auto encrypted = codec->encryptChunk(chunks[i].get(), i);
            if (encrypted) {
                encrypted_chunks.push_back(encrypted);
                logchan->log("✓ Encrypted chunk %d", i);
            } else {
                logchan->log("✗ Failed to encrypt chunk %d", i);
            }
        }

        // Decrypt each chunk
        bool all_passed = true;
        for (int i = 0; i < num_chunks; i++) {
            
            auto decrypted = codec->decryptChunk(encrypted_chunks[i].get(), i);

            if (decrypted && decrypted->length() == chunks[i]->length() &&
                memcmp(decrypted->data(), chunks[i]->data(), chunks[i]->length()) == 0) {
                logchan->log("✓ Chunk %d round-trip passed", i);
            } else {
                logchan->log("✗ Chunk %d round-trip failed", i);
                all_passed = false;
            }
        }

        if (all_passed) {
            logchan->log("✓ All chunk-based tests PASSED");
        }
    }

    // Test 4: Password-based Key Derivation
    logchan->log("");
    logchan->log("--- Test 4: Password-based Encryption ---");

    {
        std::string password = "my_secret_password_123";
        std::string context = "password_test";
        std::string plaintext = "Password-protected data";

        // Create codec from password
        auto codec = createCodec(password, context);

        auto input = std::make_shared<DataBlock>();
        input->addData((const uint8_t*)plaintext.c_str(), plaintext.length());

        // Encrypt
        auto encrypted = codec->encrypt(input.get());

        if (encrypted) {
            logchan->log("✓ Password-based encryption successful");

            // Create another codec with same password
            auto codec2 = createCodec(password, context);

            // Decrypt
            
            auto decrypted = codec2->decrypt(encrypted.get());

            if (decrypted && decrypted->length() == input->length() &&
                memcmp(decrypted->data(), input->data(), input->length()) == 0) {
                logchan->log("✓ Password-based decryption PASSED");
                std::string recovered((const char*)decrypted->data(), decrypted->length());
                logchan->log("  Recovered: '%s'", recovered.c_str());
            } else {
                logchan->log("✗ Password-based decryption FAILED");
            }
            
        } else {
            logchan->log("✗ Password-based encryption failed");
        }
    }

    // Test 6: Algorithm Info
    logchan->log("");
    logchan->log("--- Algorithm Information ---");
    logchan->log("  Algorithm: %s", LibsodiumCodec::algorithmName().c_str());
    logchan->log("  Key size:  %zu bytes", LibsodiumCodec::keySize());
    logchan->log("  Nonce size: %zu bytes", LibsodiumCodec::nonceSize());
    logchan->log("  Tag size:  %zu bytes", LibsodiumCodec::tagSize());

    logchan->log("");
    logchan->log("========================================");
    logchan->log("Crypto Tests Complete");
    logchan->log("========================================");
    logchan->log("");
    logchan->log("NOTE: This iOS implementation uses libsodium");
    logchan->log("      Same implementation as desktop/Linux");
    logchan->log("      Fully compatible encryption format");
}
