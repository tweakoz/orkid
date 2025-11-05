////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <string>
#include <vector>
#include <cstring>

using namespace ork;

////////////////////////////////////////////////////////////////
// Helper functions for test data generation
////////////////////////////////////////////////////////////////

std::vector<uint8_t> generateHashTestData(size_t size, uint8_t seed) {
    std::vector<uint8_t> data(size);
    uint8_t value = seed;
    for (size_t i = 0; i < size; i++) {
        // Simple deterministic pattern
        value = (value * 131 + 17) & 0xFF;
        data[i] = value;
    }
    return data;
}

////////////////////////////////////////////////////////////////
// Main Hash Test
////////////////////////////////////////////////////////////////

void runHashTests(void) {
    auto logchan = logger()->configureChannel("HASH", fvec3(0.6f, 0.8f, 0.2f), true);

    logchan->log("========================================");
    logchan->log("Starting Hash Tests (MD5 and XXHash)");
    logchan->log("========================================");

    // Test 1: MD5 Test
    logchan->log("");
    logchan->log("--- MD5 Tests ---");

    // Test MD5 with known string
    {
        std::string test_str = "The quick brown fox jumps over the lazy dog";
        CMD5 md5;
        md5.update((const unsigned char*)test_str.c_str(), test_str.length());
        md5.finalize();
        Md5Sum result = md5.Result();
        std::string hex = result.hex_digest();

        // Known MD5 hash of this string
        std::string expected = "9e107d9d372bb6826bd81d3542a419d6";

        if (hex == expected) {
            logchan->log("✓ MD5 string test passed");
            logchan->log("  Input: '%s'", test_str.c_str());
            logchan->log("  MD5: %s", hex.c_str());
        } else {
            logchan->log("✗ MD5 string test FAILED");
            logchan->log("  Expected: %s", expected.c_str());
            logchan->log("  Got:      %s", hex.c_str());
        }
    }

    // Test MD5 with empty string
    {
        std::string test_str = "";
        CMD5 md5;
        md5.update((const unsigned char*)test_str.c_str(), test_str.length());
        md5.finalize();
        Md5Sum result = md5.Result();
        std::string hex = result.hex_digest();

        // Known MD5 hash of empty string
        std::string expected = "d41d8cd98f00b204e9800998ecf8427e";

        if (hex == expected) {
            logchan->log("✓ MD5 empty string test passed");
            logchan->log("  MD5: %s", hex.c_str());
        } else {
            logchan->log("✗ MD5 empty string test FAILED");
            logchan->log("  Expected: %s", expected.c_str());
            logchan->log("  Got:      %s", hex.c_str());
        }
    }

    // Test MD5 with binary data (8MB)
    {
        logchan->log("");
        logchan->log("Testing MD5 with 8MB binary data...");
        auto test_data = generateHashTestData(8 * 1024 * 1024, 0x42);

        CMD5 md5;
        md5.update(test_data.data(), test_data.size());
        md5.finalize();
        Md5Sum result = md5.Result();
        std::string hex = result.hex_digest();

        logchan->log("✓ MD5 8MB binary data test completed");
        logchan->log("  Size: %.2f MB", test_data.size() / (1024.0 * 1024.0));
        logchan->log("  MD5: %s", hex.c_str());
    }

    // Test 2: XXHash64 Tests
    logchan->log("");
    logchan->log("--- XXHash64 Tests ---");

    // Test XXHash64 with known string
    {
        std::string test_str = "The quick brown fox jumps over the lazy dog";
        auto xxhasher = std::make_shared<XXH64HASH>();
        xxhasher->init();
        xxhasher->accumulateString(test_str);
        xxhasher->finish();
        uint64_t hash = xxhasher->result();

        logchan->log("✓ XXHash64 string test completed");
        logchan->log("  Input: '%s'", test_str.c_str());
        logchan->log("  XXHash64: 0x%016llx", hash);
    }

    // Test XXHash64 with empty string
    {
        std::string test_str = "";
        auto xxhasher = std::make_shared<XXH64HASH>();
        xxhasher->init();
        xxhasher->accumulateString(test_str);
        xxhasher->finish();
        uint64_t hash = xxhasher->result();

        logchan->log("✓ XXHash64 empty string test completed");
        logchan->log("  XXHash64: 0x%016llx", hash);
    }

    // Test XXHash64 with 8MB binary data
    {
        logchan->log("");
        logchan->log("Testing XXHash64 with 8MB binary data...");
        auto test_data = generateHashTestData(8 * 1024 * 1024, 0x42);

        auto xxhasher = std::make_shared<XXH64HASH>();
        xxhasher->init();
        xxhasher->accumulate(test_data.data(), test_data.size());
        xxhasher->finish();
        uint64_t hash = xxhasher->result();

        logchan->log("✓ XXHash64 8MB binary data test completed");
        logchan->log("  Size: %.2f MB", test_data.size() / (1024.0 * 1024.0));
        logchan->log("  XXHash64: 0x%016llx", hash);
    }

    // Test XXHash64 incremental hashing
    {
        logchan->log("");
        logchan->log("Testing XXHash64 incremental hashing...");

        // Generate 4MB of data
        auto full_data = generateHashTestData(4 * 1024 * 1024, 0x99);

        // Hash all at once
        auto xxhasher1 = std::make_shared<XXH64HASH>();
        xxhasher1->init();
        xxhasher1->accumulate(full_data.data(), full_data.size());
        xxhasher1->finish();
        uint64_t hash_full = xxhasher1->result();

        // Hash in 1MB chunks
        auto xxhasher2 = std::make_shared<XXH64HASH>();
        xxhasher2->init();
        size_t chunk_size = 1024 * 1024;
        for (size_t offset = 0; offset < full_data.size(); offset += chunk_size) {
            size_t remaining = full_data.size() - offset;
            size_t to_hash = (remaining < chunk_size) ? remaining : chunk_size;
            xxhasher2->accumulate(full_data.data() + offset, to_hash);
        }
        xxhasher2->finish();
        uint64_t hash_incremental = xxhasher2->result();

        if (hash_full == hash_incremental) {
            logchan->log("✓ XXHash64 incremental hashing test passed");
            logchan->log("  Full hash:        0x%016llx", hash_full);
            logchan->log("  Incremental hash: 0x%016llx", hash_incremental);
        } else {
            logchan->log("✗ XXHash64 incremental hashing test FAILED");
            logchan->log("  Full hash:        0x%016llx", hash_full);
            logchan->log("  Incremental hash: 0x%016llx", hash_incremental);
        }
    }

    // Test 3: XXHash3 Tests (if available)
    logchan->log("");
    logchan->log("--- XXHash3 Tests ---");

    // Test XXHash3 with known string
    {
        std::string test_str = "The quick brown fox jumps over the lazy dog";
        auto xxhasher = std::make_shared<XXH3HASH>();
        xxhasher->init();
        xxhasher->accumulateString(test_str);
        xxhasher->finish();
        uint64_t hash = xxhasher->result();

        logchan->log("✓ XXHash3 string test completed");
        logchan->log("  Input: '%s'", test_str.c_str());
        logchan->log("  XXHash3: 0x%016llx", hash);
    }

    // Test XXHash3 with 8MB binary data
    {
        logchan->log("");
        logchan->log("Testing XXHash3 with 8MB binary data...");
        auto test_data = generateHashTestData(8 * 1024 * 1024, 0x42);

        auto xxhasher = std::make_shared<XXH3HASH>();
        xxhasher->init();
        xxhasher->accumulate(test_data.data(), test_data.size());
        xxhasher->finish();
        uint64_t hash = xxhasher->result();

        logchan->log("✓ XXHash3 8MB binary data test completed");
        logchan->log("  Size: %.2f MB", test_data.size() / (1024.0 * 1024.0));
        logchan->log("  XXHash3: 0x%016llx", hash);
    }

    // Test 4: Hash Consistency Test
    logchan->log("");
    logchan->log("--- Hash Consistency Test ---");

    {
        // Test that same data produces same hash
        auto test_data = generateHashTestData(1024 * 1024, 0x7F);

        // MD5 consistency
        CMD5 md5a, md5b;
        md5a.update(test_data.data(), test_data.size());
        md5a.finalize();
        std::string md5_hash1 = md5a.Result().hex_digest();

        md5b.update(test_data.data(), test_data.size());
        md5b.finalize();
        std::string md5_hash2 = md5b.Result().hex_digest();

        if (md5_hash1 == md5_hash2) {
            logchan->log("✓ MD5 consistency test passed");
        } else {
            logchan->log("✗ MD5 consistency test FAILED");
        }

        // XXHash64 consistency
        auto xxh64a = std::make_shared<XXH64HASH>();
        xxh64a->init();
        xxh64a->accumulate(test_data.data(), test_data.size());
        xxh64a->finish();
        uint64_t xxh64_hash1 = xxh64a->result();

        auto xxh64b = std::make_shared<XXH64HASH>();
        xxh64b->init();
        xxh64b->accumulate(test_data.data(), test_data.size());
        xxh64b->finish();
        uint64_t xxh64_hash2 = xxh64b->result();

        if (xxh64_hash1 == xxh64_hash2) {
            logchan->log("✓ XXHash64 consistency test passed");
        } else {
            logchan->log("✗ XXHash64 consistency test FAILED");
        }
    }

    logchan->log("");
    logchan->log("========================================");
    logchan->log("Hash Tests Complete");
    logchan->log("========================================");
}
