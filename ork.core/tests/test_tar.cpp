////////////////////////////////////////////////////////////////
// Test for TarArchive functionality
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/util/tar.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/opq.h>
#include <cstdlib>
#include <atomic>

using namespace ork;
using namespace ork::util;

namespace {

// Helper to create test data file
file::Path createTestFile(const std::string& content, const std::string& filename) {
    auto temp_dir = file::Path::temp_dir() / FormatString("tar_test_%d", rand());
    temp_dir.ensureDirectoryExists();
    
    auto file_path = temp_dir / filename;
    File f(file_path, EFM_WRITE);
    f.Write(content.c_str(), content.length());
    
    return file_path;
}

// Helper to create test directory with files
file::Path createTestDirectory() {
    auto temp_dir = file::Path::temp_dir() / FormatString("tar_test_dir_%d", rand());
    temp_dir.ensureDirectoryExists();
    
    // Create some test files
    File f1(temp_dir / "file1.txt", EFM_WRITE);
    f1.Write("Content of file 1", 17);
    
    File f2(temp_dir / "file2.txt", EFM_WRITE);
    f2.Write("Content of file 2", 17);
    
    // Create subdirectory
    auto sub_dir = temp_dir / "subdir";
    sub_dir.ensureDirectoryExists();
    
    File f3(sub_dir / "file3.txt", EFM_WRITE);
    f3.Write("Content of file 3", 17);
    
    return temp_dir;
}

}

TEST(TarArchive_CreateFromMemory) {
    // Create test entries
    tar_entry_map_t entries;
    
    auto entry1 = std::make_shared<TarEntry>();
    entry1->name = "test1.txt";
    entry1->size = 11;
    entry1->mode = 0644;
    entry1->mtime = time(nullptr);
    entry1->is_directory = false;
    entry1->data = std::make_shared<DataBlock>();
    entry1->data->addData("Hello World", 11);
    
    auto entry2 = std::make_shared<TarEntry>();
    entry2->name = "test2.txt";
    entry2->size = 9;
    entry2->mode = 0644;
    entry2->mtime = time(nullptr);
    entry2->is_directory = false;
    entry2->data = std::make_shared<DataBlock>();
    entry2->data->addData("Test Data", 9);
    
    entries["test1.txt"] = entry1;
    entries["test2.txt"] = entry2;
    
    // Create archive
    TarCreateOptions options;
    auto archive = TarArchive::createFromMemory(entries, options);
    
    CHECK(archive != nullptr);
    CHECK(archive->isValid());
    CHECK_EQUAL(2, archive->listEntries().size());
    CHECK(archive->hasFile("test1.txt"));
    CHECK(archive->hasFile("test2.txt"));
}

TEST(TarArchive_ExtractToMemory) {
    // Create test entries
    tar_entry_map_t entries;
    
    auto entry = std::make_shared<TarEntry>();
    entry->name = "extract_test.txt";
    entry->size = 13;
    entry->mode = 0644;
    entry->mtime = time(nullptr);
    entry->is_directory = false;
    entry->data = std::make_shared<DataBlock>();
    entry->data->addData("Extract Test!", 13);
    
    entries["extract_test.txt"] = entry;
    
    // Create archive
    TarCreateOptions options;
    auto archive = TarArchive::createFromMemory(entries, options);
    
    CHECK(archive != nullptr);
    CHECK(archive->isValid());
    
    // Extract to memory
    TarExtractOptions extract_options;
    auto extracted_entries = archive->extractToMemory(extract_options);
    
    CHECK_EQUAL(1, extracted_entries.size());
    CHECK(extracted_entries.find("extract_test.txt") != extracted_entries.end());
    
    auto extracted_entry = extracted_entries["extract_test.txt"];
    CHECK(extracted_entry != nullptr);
    CHECK_EQUAL(13, extracted_entry->size);
    
    std::string content(reinterpret_cast<const char*>(extracted_entry->data->data()), 
                       extracted_entry->data->length());
    CHECK_EQUAL("Extract Test!", content);
}

TEST(TarArchive_ExtractSingleFile) {
    // Create test entries
    tar_entry_map_t entries;
    
    auto entry = std::make_shared<TarEntry>();
    entry->name = "single_file.txt";
    entry->size = 11;
    entry->mode = 0644;
    entry->mtime = time(nullptr);
    entry->is_directory = false;
    entry->data = std::make_shared<DataBlock>();
    entry->data->addData("Single File", 11);
    
    entries["single_file.txt"] = entry;
    
    // Create archive
    TarCreateOptions options;
    auto archive = TarArchive::createFromMemory(entries, options);
    
    CHECK(archive != nullptr);
    
    // Extract single file
    auto file_data = archive->extractFile("single_file.txt");
    CHECK(file_data != nullptr);
    CHECK_EQUAL(11, file_data->length());
    
    std::string content(reinterpret_cast<const char*>(file_data->data()), file_data->length());
    CHECK_EQUAL("Single File", content);
    
    // Try to extract non-existent file
    auto missing_file = archive->extractFile("missing.txt");
    CHECK(missing_file == nullptr);
}

TEST(TarArchive_SaveAndLoad) {
    // Create test entries
    tar_entry_map_t entries;
    
    auto entry = std::make_shared<TarEntry>();
    entry->name = "save_load_test.txt";
    entry->size = 15;
    entry->mode = 0644;
    entry->mtime = time(nullptr);
    entry->is_directory = false;
    entry->data = std::make_shared<DataBlock>();
    entry->data->addData("Save Load Test!", 15);
    
    entries["save_load_test.txt"] = entry;
    
    // Create archive
    TarCreateOptions options;
    auto original_archive = TarArchive::createFromMemory(entries, options);
    
    CHECK(original_archive != nullptr);
    
    // Save to file
    auto temp_tar = file::Path::temp_dir() / FormatString("test_%d.tar", rand());
    printf("[TEST_DEBUG] About to save to: %s\n", temp_tar.c_str());
    bool save_result = original_archive->saveToFile(temp_tar);
    printf("[TEST_DEBUG] Save result: %s\n", save_result ? "true" : "false");
    CHECK(save_result);
    printf("[TEST_DEBUG] About to check if file exists\n");
    CHECK(temp_tar.doesPathExist());
    
    // Load from file
    auto loaded_archive = TarArchive::loadFromFile(temp_tar);
    printf("[TEST_DEBUG] About to check loaded_archive != nullptr\n");
    CHECK(loaded_archive != nullptr);
    printf("[TEST_DEBUG] About to check loaded_archive->isValid()\n");
    CHECK(loaded_archive->isValid());
    printf("[TEST_DEBUG] About to check listEntries().size() == 1\n");
    CHECK_EQUAL(1, loaded_archive->listEntries().size());
    printf("[TEST_DEBUG] About to check hasFile('save_load_test.txt')\n");
    CHECK(loaded_archive->hasFile("save_load_test.txt"));
    
    // Extract and verify content
    auto extracted_data = loaded_archive->extractFile("save_load_test.txt");
    printf("[TEST_DEBUG] extracted_data: %p\n", extracted_data.get());
    if (extracted_data == nullptr) {
        printf("[TEST_DEBUG] ERROR: extracted_data is null!\n");
    }
    CHECK(extracted_data != nullptr);
    
    printf("[TEST_DEBUG] extracted_data->length(): %zu\n", extracted_data ? extracted_data->length() : 0);
    if (extracted_data && extracted_data->length() != 15) {
        printf("[TEST_DEBUG] ERROR: length mismatch! Expected 15, got %zu\n", extracted_data->length());
    }
    CHECK_EQUAL(15, extracted_data->length());
    
    std::string content(reinterpret_cast<const char*>(extracted_data->data()), 
                       extracted_data->length());
    printf("[TEST_DEBUG] content: '%s'\n", content.c_str());
    printf("[TEST_DEBUG] content.length(): %zu\n", content.length());
    
    // Debug each character
    printf("[TEST_DEBUG] content bytes: ");
    for (size_t i = 0; i < content.length(); i++) {
        printf("%02x ", (unsigned char)content[i]);
    }
    printf("\n");
    
    // Test string comparison manually
    std::string expected = "Save Load Test!";
    printf("[TEST_DEBUG] expected: '%s', length: %zu\n", expected.c_str(), expected.length());
    printf("[TEST_DEBUG] strings equal: %s\n", (content == expected) ? "YES" : "NO");
    
    CHECK_EQUAL("Save Load Test!", content);
    
    // Clean up (temp files will be cleaned up automatically)
}

TEST(TarArchive_ConcurrentOperations) {
    using namespace ork::opq;
    
    // Create multiple test data sets
    const int num_archives = 5;
    std::vector<tar_entry_map_t> test_data(num_archives);
    std::vector<file::Path> temp_files(num_archives);
    
    // Prepare test data
    for (int i = 0; i < num_archives; i++) {
        auto entry = std::make_shared<TarEntry>();
        entry->name = FormatString("concurrent_test_%d.txt", i);
        entry->size = 20 + i;
        entry->mode = 0644;
        entry->mtime = time(nullptr);
        entry->is_directory = false;
        entry->data = std::make_shared<DataBlock>();
        
        std::string content = FormatString("Concurrent test data %d!", i);
        entry->data->addData(content.c_str(), content.length());
        entry->size = content.length(); // Fix: Update size to match actual content length
        
        test_data[i][entry->name] = entry;
        temp_files[i] = file::Path::temp_dir() / FormatString("concurrent_test_%d.tar", i);
    }
    
    // Create operation queue for concurrent operations
    auto opq = std::make_shared<OperationsQueue>(4, "TarTest");
    
    // Results storage
    std::vector<std::atomic<bool>> create_results(num_archives);
    std::vector<std::atomic<bool>> save_results(num_archives);
    std::vector<std::atomic<bool>> load_results(num_archives);
    std::vector<std::atomic<bool>> verify_results(num_archives);
    
    for (int i = 0; i < num_archives; i++) {
        create_results[i] = false;
        save_results[i] = false; 
        load_results[i] = false;
        verify_results[i] = false;
    }
    
    printf("[TEST_DEBUG] Starting concurrent tar operations with %d archives\n", num_archives);
    
    // Phase 1: Create archives concurrently
    for (int i = 0; i < num_archives; i++) {
        opq->enqueue([i, &test_data, &create_results]() {
            printf("[TEST_DEBUG] Creating archive %d\n", i);
            TarCreateOptions options;
            auto archive = TarArchive::createFromMemory(test_data[i], options);
            create_results[i] = (archive != nullptr && archive->isValid());
            printf("[TEST_DEBUG] Archive %d creation: %s\n", i, create_results[i].load() ? "SUCCESS" : "FAILED");
        });
    }
    
    // Wait for all creates to complete
    opq->drain();
    
    // Verify all creates succeeded
    for (int i = 0; i < num_archives; i++) {
        CHECK(create_results[i].load());
    }
    
    // Phase 2: Save archives to files concurrently
    std::vector<tararchive_ptr_t> archives(num_archives);
    
    // First recreate archives for saving
    for (int i = 0; i < num_archives; i++) {
        TarCreateOptions options;
        archives[i] = TarArchive::createFromMemory(test_data[i], options);
        CHECK(archives[i] != nullptr);
    }
    
    // Save concurrently
    for (int i = 0; i < num_archives; i++) {
        opq->enqueue([i, &archives, &temp_files, &save_results]() {
            printf("[TEST_DEBUG] Saving archive %d to file\n", i);
            bool result = archives[i]->saveToFile(temp_files[i]);
            save_results[i] = result;
            printf("[TEST_DEBUG] Archive %d save: %s\n", i, result ? "SUCCESS" : "FAILED");
        });
    }
    
    // Wait for all saves to complete
    opq->drain();
    
    // Verify all saves succeeded
    for (int i = 0; i < num_archives; i++) {
        CHECK(save_results[i].load());
        CHECK(temp_files[i].doesPathExist());
    }
    
    // Phase 3: Load archives from files concurrently
    std::vector<tararchive_ptr_t> loaded_archives(num_archives);
    
    for (int i = 0; i < num_archives; i++) {
        opq->enqueue([i, &temp_files, &loaded_archives, &load_results]() {
            printf("[TEST_DEBUG] Loading archive %d from file\n", i);
            auto archive = TarArchive::loadFromFile(temp_files[i]);
            loaded_archives[i] = archive;
            bool result = (archive != nullptr && archive->isValid());
            load_results[i] = result;
            printf("[TEST_DEBUG] Archive %d load: %s\n", i, result ? "SUCCESS" : "FAILED");
        });
    }
    
    // Wait for all loads to complete
    opq->drain();
    
    // Verify all loads succeeded
    for (int i = 0; i < num_archives; i++) {
        CHECK(load_results[i].load());
        CHECK(loaded_archives[i] != nullptr);
        CHECK(loaded_archives[i]->isValid());
    }
    
    // Phase 4: Verify content concurrently
    for (int i = 0; i < num_archives; i++) {
        opq->enqueue([i, &loaded_archives, &verify_results]() {
            printf("[TEST_DEBUG] Verifying archive %d content\n", i);
            
            std::string expected_filename = FormatString("concurrent_test_%d.txt", i);
            std::string expected_content = FormatString("Concurrent test data %d!", i);
            
            // Check file exists in archive
            if (!loaded_archives[i]->hasFile(expected_filename)) {
                printf("[TEST_DEBUG] Archive %d missing expected file: %s\n", i, expected_filename.c_str());
                verify_results[i] = false;
                return;
            }
            
            // Extract and verify content
            auto extracted_data = loaded_archives[i]->extractFile(expected_filename);
            if (!extracted_data) {
                printf("[TEST_DEBUG] Archive %d failed to extract file: %s\n", i, expected_filename.c_str());
                verify_results[i] = false;
                return;
            }
            
            std::string actual_content(reinterpret_cast<const char*>(extracted_data->data()), 
                                     extracted_data->length());
            
            if (actual_content != expected_content) {
                printf("[TEST_DEBUG] Archive %d content mismatch. Expected: '%s', Got: '%s'\n", 
                       i, expected_content.c_str(), actual_content.c_str());
                verify_results[i] = false;
                return;
            }
            
            verify_results[i] = true;
            printf("[TEST_DEBUG] Archive %d verification: SUCCESS\n", i);
        });
    }
    
    // Wait for all verifications to complete
    opq->drain();
    
    // Verify all content checks succeeded
    for (int i = 0; i < num_archives; i++) {
        CHECK(verify_results[i].load());
    }
    
    printf("[TEST_DEBUG] All concurrent tar operations completed successfully\n");
    
    // Clean up (temp files will be cleaned up automatically)
}

TEST(TarArchive_CreateFromFiles) {
    // Create test files
    auto test_file1 = createTestFile("File 1 Content", "file1.txt");
    auto test_file2 = createTestFile("File 2 Content", "file2.txt");
    
    std::vector<file::Path> file_paths = { test_file1, test_file2 };
    
    // Create archive from files
    TarCreateOptions options;
    auto archive = TarArchive::createFromFiles(file_paths, options);
    
    CHECK(archive != nullptr);
    CHECK(archive->isValid());
    CHECK_EQUAL(2, archive->listEntries().size());
    
    // Verify files exist in archive
    auto entries = archive->listEntries();
    bool found_file1 = false, found_file2 = false;
    for (const auto& entry_name : entries) {
        if (entry_name.find("file1.txt") != std::string::npos) found_file1 = true;
        if (entry_name.find("file2.txt") != std::string::npos) found_file2 = true;
    }
    CHECK(found_file1);
    CHECK(found_file2);
}

TEST(TarArchive_UtilityFunctions) {
    // Create test file
    auto test_file = createTestFile("Utility Test", "utility.txt");
    
    // Create tar from single file
    TarCreateOptions options;
    auto archive = createTarFromSingleFile(test_file, "utility_test.tar", options);
    
    CHECK(archive != nullptr);
    CHECK(archive->isValid());
    CHECK_EQUAL(1, archive->listEntries().size());
    
    // Save to temp file
    auto temp_tar = file::Path::temp_dir() / "utility_test.tar";
    CHECK(archive->saveToFile(temp_tar));
    
    // Test utility functions
    auto contents = listTarContents(temp_tar);
    CHECK_EQUAL(1, contents.size());
    
    auto file_size = getTarFileSize(temp_tar);
    CHECK(file_size > 0);
    
    auto extracted_data = extractSingleFileFromTar(temp_tar, contents[0]);
    CHECK(extracted_data != nullptr);
    
    std::string content(reinterpret_cast<const char*>(extracted_data->data()), 
                       extracted_data->length());
    CHECK_EQUAL("Utility Test", content);
    
    // Clean up (temp files will be cleaned up automatically)
}