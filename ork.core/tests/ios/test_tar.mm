////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <ork/util/tar.h>
#include <ork/kernel/datablock.h>
#include <string>
#include <vector>
#include <cstring>

using namespace ork;
using namespace ork::util;

////////////////////////////////////////////////////////////////
// Helper functions for synthetic data generation
////////////////////////////////////////////////////////////////

std::vector<uint8_t> generatePatternData(size_t size, uint8_t pattern) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; i++) {
        data[i] = pattern;
    }
    return data;
}

std::vector<uint8_t> generateSequenceData(size_t size) {
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; i++) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }
    return data;
}

std::vector<uint8_t> generatePseudoRandomData(size_t size, uint32_t seed) {
    std::vector<uint8_t> data(size);
    uint32_t state = seed;
    for (size_t i = 0; i < size; i++) {
        // Simple LCG pseudo-random generator
        state = state * 1103515245 + 12345;
        data[i] = static_cast<uint8_t>((state >> 16) & 0xFF);
    }
    return data;
}

bool compareData(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    return memcmp(a.data(), b.data(), a.size()) == 0;
}

////////////////////////////////////////////////////////////////
// Main TAR Test - Fully In-Memory
////////////////////////////////////////////////////////////////

void runTarTests(void) {
    auto logchan = logger()->configureChannel("TAR", fvec3(0.8f, 0.6f, 0.2f), true);

    logchan->log("========================================");
    logchan->log("Starting libtar Tests (In-Memory)");
    logchan->log("========================================");

    // Test data structure
    struct TestFile {
        std::string name;
        std::vector<uint8_t> data;
    };

    std::vector<TestFile> testFiles;

    // Test 1: Generate synthetic test files (32MB+ total)
    logchan->log("");
    logchan->log("--- Generating Test Data ---");

    // File 1: 8MB of pattern 0xAA
    testFiles.push_back({
        "pattern_aa.bin",
        generatePatternData(8 * 1024 * 1024, 0xAA)
    });
    logchan->log("Generated: pattern_aa.bin (8 MB, pattern 0xAA)");

    // File 2: 8MB of pattern 0x55
    testFiles.push_back({
        "pattern_55.bin",
        generatePatternData(8 * 1024 * 1024, 0x55)
    });
    logchan->log("Generated: pattern_55.bin (8 MB, pattern 0x55)");

    // File 3: 8MB of sequence data (0-255 repeating)
    testFiles.push_back({
        "sequence.bin",
        generateSequenceData(8 * 1024 * 1024)
    });
    logchan->log("Generated: sequence.bin (8 MB, sequence)");

    // File 4: 8MB of pseudo-random data
    testFiles.push_back({
        "random.bin",
        generatePseudoRandomData(8 * 1024 * 1024, 0x12345678)
    });
    logchan->log("Generated: random.bin (8 MB, pseudo-random)");

    // File 5: Small text file
    std::string textContent = "This is a test file for libtar iOS testing.\n";
    textContent += "It contains multiple lines of text.\n";
    textContent += "Testing compression and extraction.\n";
    testFiles.push_back({
        "readme.txt",
        std::vector<uint8_t>(textContent.begin(), textContent.end())
    });
    logchan->log("Generated: readme.txt (%zu bytes, text)", textContent.size());

    size_t totalSize = 0;
    for (const auto& tf : testFiles) {
        totalSize += tf.data.size();
    }
    logchan->log("Total test data: %.2f MB", totalSize / (1024.0 * 1024.0));

    // Test 2: Create TAR entries in memory
    logchan->log("");
    logchan->log("--- Creating TAR Entries ---");

    tar_entry_map_t entries;

    for (const auto& tf : testFiles) {
        auto entry = std::make_shared<TarEntry>();
        entry->name = tf.name;
        entry->size = tf.data.size();
        entry->mode = 0644;
        entry->mtime = time(nullptr);
        entry->is_directory = false;

        // Create DataBlock from vector
        entry->data = std::make_shared<DataBlock>();
        entry->data->reserve(tf.data.size());
        entry->data->_storage.resize(tf.data.size());
        memcpy(const_cast<uint8_t*>(entry->data->data()), tf.data.data(), tf.data.size());

        entries[entry->name] = entry;
        logchan->log("Created entry: %s (%zu bytes)", entry->name.c_str(), entry->size);
    }

    logchan->log("✓ All entries created in memory");

    // Test 3: Create TAR archive in memory
    logchan->log("");
    logchan->log("--- Creating TAR Archive (In-Memory) ---");

    TarCreateOptions create_options;
    auto archive = TarArchive::createFromMemory(entries, create_options);

    if (!archive) {
        logchan->log("✗ Failed to create TAR archive");
        return;
    }

    logchan->log("✓ TAR archive created successfully");
    logchan->log("Archive size: %.2f MB", archive->getArchiveSize() / (1024.0 * 1024.0));

    // Test 4: Extract TAR archive to memory
    logchan->log("");
    logchan->log("--- Extracting TAR Archive (In-Memory) ---");

    TarExtractOptions extract_options;
    auto extracted_entries = archive->extractToMemory(extract_options);

    if (extracted_entries.empty()) {
        logchan->log("✗ Failed to extract archive");
        return;
    }

    logchan->log("✓ Archive extracted successfully");
    logchan->log("Extracted %zu entries", extracted_entries.size());

    // Test 5: Verify extracted data
    logchan->log("");
    logchan->log("--- Verifying Extracted Data ---");

    bool allMatch = true;
    size_t totalVerified = 0;

    for (const auto& tf : testFiles) {
        auto it = extracted_entries.find(tf.name);
        if (it == extracted_entries.end()) {
            logchan->log("✗ Missing entry: %s", tf.name.c_str());
            allMatch = false;
            continue;
        }

        auto extracted_entry = it->second;
        if (!extracted_entry->data) {
            logchan->log("✗ No data for entry: %s", tf.name.c_str());
            allMatch = false;
            continue;
        }

        // Convert DataBlock to vector for comparison
        std::vector<uint8_t> extractedData(
            extracted_entry->data->data(),
            extracted_entry->data->data() + extracted_entry->data->length()
        );

        if (!compareData(tf.data, extractedData)) {
            logchan->log("✗ Data mismatch: %s (original: %zu bytes, extracted: %zu bytes)",
                        tf.name.c_str(), tf.data.size(), extractedData.size());
            allMatch = false;
        } else {
            logchan->log("✓ Verified: %s (%zu bytes match)", tf.name.c_str(), tf.data.size());
            totalVerified += tf.data.size();
        }
    }

    if (allMatch) {
        logchan->log("");
        logchan->log("✓ All files verified successfully!");
        logchan->log("Total verified: %.2f MB", totalVerified / (1024.0 * 1024.0));
    } else {
        logchan->log("");
        logchan->log("✗ Some files failed verification");
    }

    // Test 6: List entries
    logchan->log("");
    logchan->log("--- Listing Archive Contents ---");
    auto entry_list = archive->listEntries();
    logchan->log("Archive contains %zu files:", entry_list.size());
    for (const auto& name : entry_list) {
        auto info = archive->getEntryInfo(name);
        if (info) {
            logchan->log("  - %s (%zu bytes)", name.c_str(), info->size);
        }
    }

    logchan->log("");
    logchan->log("========================================");
    logchan->log("libtar Tests Complete");
    logchan->log("========================================");
}
