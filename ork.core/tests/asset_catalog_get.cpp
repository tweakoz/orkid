////////////////////////////////////////////////////////////////
// Test for AssetCatalog get() and getAsync() methods
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/config.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/kernel/mutex.h>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <boost/filesystem.hpp>

using namespace ork;
using namespace ork::asset::catalog;

namespace {

// Helper to create test config space
assetconfigspace_ptr_t createTestConfigSpace() {
    auto cfgspc = std::make_shared<AssetConfigSpace>();
    auto config = std::make_shared<AssetConfig>();
    
    // Add test namespace with local storage
    config->addNamespace("test", "test_password", "local_test");
    config->addRemoteLocation("local_test", "file://localhost/tmp");
    config->addLocalLocation("cache", "<assetcache>");
    config->addLocalLocation("stage", "<stage>");
    
    cfgspc->_configs["test"] = config;
    cfgspc->markDirty();
    
    return cfgspc;
}

// Helper to create test data file
file::Path createTestAssetFile(const std::string& content, const std::string& filename, const file::Path& base_dir) {
    auto file_path = base_dir / filename;
    File f(file_path, EFM_WRITE);
    f.Write(content.c_str(), content.length());
    
    return file_path;
}


}

TEST(AssetCatalog_Get_BasicSync) {
    // Create catalog with config space
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    
    // Create test directory
    auto temp_dir = file::Path::temp_dir() / FormatString("catalog_test_%d", rand());
    temp_dir.ensureDirectoryExists();
    
    // This test would require a running server or mock HTTP client
    // For now, just test that the API returns appropriate error for missing assets
    auto result = catalog->get("test::test.txt", false);
    CHECK(result != nullptr);
    CHECK(!result->isSuccess());
    CHECK(result->status == AssetStatus::NOT_FOUND);
}

TEST(AssetCatalog_Get_NotFound) {
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    
    // Try to get non-existent asset
    auto result = catalog->get("test::nonexistent.txt", false);
    CHECK(result != nullptr);
    CHECK(!result->isSuccess());
    //CHECK_EQUAL(AssetStatus::NOT_FOUND, result->status);
    CHECK(result->data == nullptr);
    CHECK(!result->error_detail.empty());
}

TEST(AssetCatalog_Get_CacheHit) {
    // This test would require actual remote assets to test caching
    // For now, we'll just verify the catalog is created properly
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    
    // Verify catalog directories are created
    CHECK(catalog->getCacheDir().doesPathExist());
    CHECK(catalog->getEncryptedDir().doesPathExist());
    CHECK(catalog->getChunksDir().doesPathExist());
}

TEST(AssetCatalog_Get_Compressed) {
    // Test compression/decompression functionality directly
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    
    // Test data compression
    std::string original_data;
    for (int i = 0; i < 100; ++i) {
        original_data += "This is test data that compresses well! ";
    }
    
    auto data_block = std::make_shared<DataBlock>();
    data_block->addData(original_data.c_str(), original_data.length());
    auto compressed = data_block->compressed();
    
    CHECK(compressed != nullptr);
    CHECK(compressed->length() < original_data.length());
    
    // Test decompression
    auto decompressed = compressed->decompressed();
    CHECK(decompressed != nullptr);
    CHECK_EQUAL(original_data.length(), decompressed->length());
}

TEST(AssetCatalog_NamespaceCodec) {
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    
    // Register codec for namespace
    catalog->registerCodecWithPassword("secure", "test_password");
    
    // Check codec exists
    auto codec = catalog->codecForNamespace("secure");
    CHECK(codec != nullptr);
    
    // Test encryption/decryption
    std::string test_data = "Secret Data";
    auto data_block = std::make_shared<DataBlock>();
    data_block->addData(test_data.c_str(), test_data.length());
    
    auto encrypted = codec->encrypt(data_block.get());
    CHECK(encrypted != nullptr);
    CHECK(encrypted->length() > 0);
    
    auto decrypted = codec->decrypt(encrypted.get());
    CHECK(decrypted != nullptr);
    CHECK_EQUAL(test_data.length(), decrypted->length());
    
    std::string decrypted_str(reinterpret_cast<const char*>(decrypted->data()), decrypted->length());
    CHECK_EQUAL(test_data, decrypted_str);
}