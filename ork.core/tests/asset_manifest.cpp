////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/types.h>
#include <ork/file/path.h>
#include <ork/kernel/timer.h>
#include <ork/util/crypt.h>
#include <fstream>
#include <sstream>

using namespace ork;
using namespace ork::asset::catalog;
using namespace ork::file;

namespace {

////////////////////////////////////////////////////////////////////////////////
// Test Helpers
////////////////////////////////////////////////////////////////////////////////

Path create_test_directory_structure() {
    auto test_dir = Path::temp_dir() / "asset_manifest_test";
    test_dir.ensureDirectoryExists();
    
    // Create subdirectories
    (test_dir / "textures").ensureDirectoryExists();
    (test_dir / "models").ensureDirectoryExists();
    (test_dir / "sounds").ensureDirectoryExists();
    (test_dir / "scripts").ensureDirectoryExists();
    
    // Create test files
    std::ofstream((test_dir / "textures" / "texture1.png").c_str()).close();
    std::ofstream((test_dir / "textures" / "texture2.jpg").c_str()).close();
    std::ofstream((test_dir / "models" / "model1.obj").c_str()).close();
    std::ofstream((test_dir / "models" / "model2.fbx").c_str()).close();
    std::ofstream((test_dir / "sounds" / "sound1.wav").c_str()).close();
    std::ofstream((test_dir / "scripts" / "script1.lua").c_str()).close();
    
    return test_dir;
}

void write_test_file(const Path& path, const std::string& content) {
    std::ofstream file(path.c_str());
    file << content;
    file.close();
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// AssetManifest Merge Tests
////////////////////////////////////////////////////////////////////////////////

TEST(AssetManifest_Merge) {
    auto manifest1 = std::make_shared<AssetManifest>();
    manifest1->setNamespace("base");
    
    auto entry1 = std::make_shared<AssetEntry>();
    manifest1->addAsset("asset1", entry1);
    
    auto entry2 = std::make_shared<AssetEntry>();
    manifest1->addAsset("asset2", entry2);
    
    // Create second manifest
    auto manifest2 = std::make_shared<AssetManifest>();
    manifest2->setNamespace("addon");
    
    auto entry3 = std::make_shared<AssetEntry>();
    manifest2->addAsset("asset3", entry3);
    
    // Merge
    manifest1->merge(*manifest2);
    
    // Should have all assets
    auto& assets = manifest1->getAssets();
    CHECK(assets.size() == 3);
    CHECK(assets.find("asset1") != assets.end());
    CHECK(assets.find("asset2") != assets.end());
    CHECK(assets.find("asset3") != assets.end());
}

////////////////////////////////////////////////////////////////////////////////
// Cleanup
////////////////////////////////////////////////////////////////////////////////

TEST(AssetManifest_Cleanup) {
    // Clean up test directory
    auto temp_dir = Path::temp_dir() / "asset_manifest_test";
    if (temp_dir.doesPathExist()) {
        // Cleanup handled by OS
    }
}