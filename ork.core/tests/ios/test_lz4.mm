////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <lz4.h>
#include <lz4hc.h>
#include <string>
#include <vector>
#include <cstring>

using namespace ork;

void runLZ4Tests(void) {
    auto logchan = logger()->configureChannel("LZ4TEST", fvec3(0.5f, 1.0f, 0.5f), true);

    logchan->log("========================================");
    logchan->log("Starting LZ4 Tests");
    logchan->log("========================================");

    // Create test data
    const char* testString = "Hello, Orkid! This is a test of LZ4 compression. "
                             "LZ4 is a lossless compression algorithm that provides "
                             "very fast compression and decompression speeds. "
                             "This string is repeated to make it more compressible. "
                             "Hello, Orkid! This is a test of LZ4 compression. "
                             "LZ4 is a lossless compression algorithm that provides "
                             "very fast compression and decompression speeds.";

    size_t originalSize = strlen(testString) + 1; // +1 for null terminator

    logchan->log("");
    logchan->log("--- LZ4 Compression Test ---");
    logchan->log("Original data size: %zu bytes", originalSize);
    logchan->log("Original data: \"%s\"", testString);

    // Allocate compression buffer
    int maxCompressedSize = LZ4_compressBound(originalSize);
    std::vector<char> compressedData(maxCompressedSize);

    logchan->log("Max compressed size: %d bytes", maxCompressedSize);

    // Compress the data
    int compressedSize = LZ4_compress_default(
        testString,
        compressedData.data(),
        originalSize,
        maxCompressedSize
    );

    if (compressedSize <= 0) {
        logchan->log("ERROR: Compression failed!");
        logchan->log("========================================");
        return;
    }

    logchan->log("Compressed size: %d bytes", compressedSize);
    logchan->log("Compression ratio: %.2f%%", (float)compressedSize / originalSize * 100.0f);
    logchan->log("Space saved: %zu bytes (%.1f%%)",
                 originalSize - compressedSize,
                 (1.0f - (float)compressedSize / originalSize) * 100.0f);

    // Decompress the data
    logchan->log("");
    logchan->log("--- LZ4 Decompression Test ---");

    std::vector<char> decompressedData(originalSize);

    int decompressedSize = LZ4_decompress_safe(
        compressedData.data(),
        decompressedData.data(),
        compressedSize,
        originalSize
    );

    if (decompressedSize < 0) {
        logchan->log("ERROR: Decompression failed! (error code: %d)", decompressedSize);
        logchan->log("========================================");
        return;
    }

    logchan->log("Decompressed size: %d bytes", decompressedSize);

    // Verify the data
    if (decompressedSize == (int)originalSize &&
        memcmp(testString, decompressedData.data(), originalSize) == 0) {
        logchan->log("✓ Decompression successful - data matches original!");
    } else {
        logchan->log("✗ ERROR: Decompressed data does not match original!");
        logchan->log("  Expected size: %zu, got: %d", originalSize, decompressedSize);
    }

    // Test high compression mode
    logchan->log("");
    logchan->log("--- LZ4 HC (High Compression) Test ---");

    std::vector<char> hcCompressedData(maxCompressedSize);

    int hcCompressedSize = LZ4_compress_HC(
        testString,
        hcCompressedData.data(),
        originalSize,
        maxCompressedSize,
        LZ4HC_CLEVEL_DEFAULT
    );

    if (hcCompressedSize <= 0) {
        logchan->log("ERROR: HC compression failed!");
    } else {
        logchan->log("HC compressed size: %d bytes", hcCompressedSize);
        logchan->log("HC compression ratio: %.2f%%", (float)hcCompressedSize / originalSize * 100.0f);
        logchan->log("HC vs normal: %d bytes %s",
                     abs(hcCompressedSize - compressedSize),
                     hcCompressedSize < compressedSize ? "smaller" : "larger");

        // Decompress HC data
        std::vector<char> hcDecompressedData(originalSize);
        int hcDecompressedSize = LZ4_decompress_safe(
            hcCompressedData.data(),
            hcDecompressedData.data(),
            hcCompressedSize,
            originalSize
        );

        if (hcDecompressedSize == (int)originalSize &&
            memcmp(testString, hcDecompressedData.data(), originalSize) == 0) {
            logchan->log("✓ HC decompression successful - data matches original!");
        } else {
            logchan->log("✗ ERROR: HC decompressed data does not match!");
        }
    }

    // Test with different data patterns
    logchan->log("");
    logchan->log("--- Pattern Compression Tests ---");

    // Highly compressible data (all zeros)
    std::vector<char> zeros(1024, 0);
    std::vector<char> zerosCompressed(LZ4_compressBound(1024));
    int zerosCompSize = LZ4_compress_default(zeros.data(), zerosCompressed.data(), 1024, zerosCompressed.size());
    logchan->log("1024 zeros: compressed to %d bytes (%.2f%%)",
                 zerosCompSize, (float)zerosCompSize / 1024 * 100.0f);

    // Less compressible data (random-ish pattern)
    std::vector<char> pattern(1024);
    for (size_t i = 0; i < pattern.size(); i++) {
        pattern[i] = (i * 37 + i * i) & 0xFF; // Pseudo-random pattern
    }
    std::vector<char> patternCompressed(LZ4_compressBound(1024));
    int patternCompSize = LZ4_compress_default(pattern.data(), patternCompressed.data(), 1024, patternCompressed.size());
    logchan->log("1024 pseudo-random: compressed to %d bytes (%.2f%%)",
                 patternCompSize, (float)patternCompSize / 1024 * 100.0f);

    logchan->log("");
    logchan->log("========================================");
    logchan->log("LZ4 Tests Complete");
    logchan->log("========================================");
}
