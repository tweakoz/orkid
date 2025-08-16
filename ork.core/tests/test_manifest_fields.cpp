////////////////////////////////////////////////////////////////
// Test for AssetManifest field renaming
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/kernel/string/deco.inl>

using namespace ork;
using namespace ork::asset::catalog;

TEST(AssetManifest_FieldRenaming) {
    // Test 1: Create AssetEntry with new field names
    auto entry = std::make_shared<AssetEntry>();
    entry->_type = "asset";
    entry->_local_loc = "/cache/test_asset";
    entry->_remote_loc = "https://cdn.example.com/assets";
    entry->_storage_hash = "abc123";
    
    CHECK_EQUAL("asset", entry->_type);
    CHECK_EQUAL("/cache/test_asset", entry->_local_loc);
    CHECK_EQUAL("https://cdn.example.com/assets", entry->_remote_loc);
    CHECK_EQUAL("abc123", entry->_storage_hash);
}

TEST(AssetManifest_JSONSerialization) {
    // Create manifest with new fields
    auto manifest = std::make_shared<AssetManifest>();
    manifest->setNamespace("test_namespace");
    manifest->setVersion("1.0.0");
    
    auto entry = std::make_shared<AssetEntry>();
    entry->_type = "asset";
    entry->_local_loc = "/local/path";
    entry->_remote_loc = "/remote/path";
    entry->_storage_hash = "12345";
    
    manifest->addAsset("test_asset", entry);
    
    // Convert to JSON
    std::string json = manifest->toJson();
    
    // Check that JSON contains new field names
    CHECK(json.find("\"local_loc\"") != std::string::npos);
    CHECK(json.find("\"remote_loc\"") != std::string::npos);
    
    // Check that JSON does NOT contain old field names
    CHECK(json.find("\"dst_loc\"") == std::string::npos);
    CHECK(json.find("\"src_loc\"") == std::string::npos);
}

TEST(AssetManifest_JSONParsing) {
    // JSON with new field names
    std::string json = R"({
        "namespace": "test",
        "version": "1.0.0",
        "assets": {
            "asset1": {
                "type": "asset",
                "priority": 100,
                "merge": false,
                "local_loc": "/local/asset1",
                "remote_loc": "/remote/asset1",
                "filename": "asset1.dat",
                "md5": "hash123"
            }
        }
    })";
    
    auto manifest = AssetManifest::parseFromString(json, file::Path("/tmp/test.json"));
    CHECK(manifest != nullptr);
    
    auto& assets = manifest->getAssets();
    CHECK_EQUAL(1, assets.size());
    
    auto it = assets.find("asset1");
    CHECK(it != assets.end());
    
    auto& entry = it->second;
    CHECK_EQUAL("/local/asset1", entry->_local_loc);
    CHECK_EQUAL("/remote/asset1", entry->_remote_loc);
    CHECK_EQUAL("hash123", entry->_storage_hash);
}

TEST(AssetManifest_AssetPakType) {
    auto entry = std::make_shared<AssetEntry>();
    entry->_type = "asset_pak";
    entry->_local_loc = "/cache/models";  // Where to extract
    entry->_remote_loc = "https://cdn/paks";  // Where to download from
    entry->_storage_hash = "abcdef";
    
    CHECK_EQUAL("asset_pak", entry->_type);
    CHECK_EQUAL("/cache/models", entry->_local_loc);
    CHECK_EQUAL("https://cdn/paks", entry->_remote_loc);
}