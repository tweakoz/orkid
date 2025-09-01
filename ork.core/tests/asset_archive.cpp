////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/chunk_manifest.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/util/tar.h>
#include <ork/file/file.h>
#include <ork/util/crypt.h>
#include <ork/kernel/datablock.h>
#include <utpp/UnitTest++.h>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <fstream>

using namespace ork;
using namespace ork::asset::catalog;
using namespace ork::util;

namespace {

// Helper to create test pak directory structure
file::Path createTestPakDirectory() {
    // Create parent directory
    auto temp_dir = file::Path::temp_dir() / FormatString("pak_test_%d", rand());
    temp_dir.ensureDirectoryExists();
    
    // Create the pak directory (without .tar extension)
    auto pak_dir = temp_dir / "test_pak";
    pak_dir.ensureDirectoryExists();
    
    // Create subdirectories
    (pak_dir / "models").ensureDirectoryExists();
    (pak_dir / "textures").ensureDirectoryExists();
    
    // Write test files to the directory structure
    File::writeString(pak_dir / "config.json", "{\"name\": \"test_asset\"}");
    File::writeString(pak_dir / "models" / "character.obj", "# OBJ file\nv 0.0 0.0 0.0\n");
    File::writeString(pak_dir / "textures" / "skin.png", "PNG data here");
    
    // Return the parent directory (not the pak directory itself)
    return temp_dir;
}

// Helper to create proper test setup with ConfigSpace
assetconfigspace_ptr_t createTestConfigSpace() {
    auto cfgspc = std::make_shared<AssetConfigSpace>();
    
    // Create test config
    auto temp_dir = file::Path::temp_dir() / FormatString("test_config_%d", rand());
    temp_dir.ensureDirectoryExists();
    
    auto config_file = temp_dir / "test_config.json";
    std::string config_json = R"({
        "namespaces": {
            "pak_test": {
                "encryption_key": "test_key",
                "upload_location": "test_remote"
            }
        },
        "locations": {
            "test_remote": "file://" + temp_dir.toStdString() + "/remote"
        },
        "destinations": {
            "cache": "<assetcache>",
            "stage": "<stage>"
        }
    })";
    
    File::writeString(config_file, config_json);
    
    auto cfg = cfgspc->createConfig("test1", config_file);
    return cfgspc;
}


TEST(AssetArchivePakCreation) {
    // Create config space and catalog
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    
    // Set cache directory
    auto cache_dir = file::Path::temp_dir() / FormatString("test_cache_%d", rand());
    catalog->setCacheDir(cache_dir);
    
    // Register codec for namespace
    catalog->registerCodecWithPassword("pak_test", "test_password");
    
    // Create test pak directory structure
    auto pak_parent_dir = createTestPakDirectory();
    
    // Create manifest using builder pattern
    auto manifest = AssetCatalog::createManifest(
        catalog,
        "test_manifest",
        "1.0",
        "pak_test",
        pak_parent_dir / "manifest.json"
    );
    
    // Create asset using builder pattern
    platform_list_t platforms = {"mac", "linux"};
    assetid_list_t dependencies;
    
    auto asset = AssetManifest::createAsset(
        manifest,
        "test_pak",
        100,
        "asset_pak",
        pak_parent_dir.c_str(),  // Pass parent directory containing test_pak/
        platforms,
        dependencies,
        "test_pak"  // tar_root
    );
    
    // Verify asset was created properly
    CHECK(asset != nullptr);
    CHECK(asset->_type == "asset_pak");
    
    // Repackage to compute hashes and encrypt
    asset->repackage();
    
    // Verify hashes were computed
    CHECK(!asset->_content_hash.empty());
    CHECK(!asset->_storage_hash.empty());
    CHECK(asset->_content_hash != asset->_storage_hash); // Should differ due to encryption
    
    // Verify encrypted file was created
    auto encrypted_path = asset->getLocalEncryptedPath();
    CHECK(encrypted_path.doesPathExist());
    
    // Test idempotent repackaging
    auto old_storage_hash = asset->_storage_hash;
    asset->repackage();
    CHECK(asset->_storage_hash == old_storage_hash);
    
    // Decrypt and verify content
    auto codec = catalog->codecForNamespace("pak_test");
    CHECK(codec != nullptr);
    
    auto encrypted_data = File::loadDatablock(encrypted_path);
    CHECK(encrypted_data != nullptr);
    
    auto decrypted_data = codec->decrypt(encrypted_data.get());
    CHECK(decrypted_data != nullptr);
    
    // Verify decrypted data is a valid TAR
    auto archive = TarArchive::loadFromMemory(decrypted_data);
    CHECK(archive != nullptr);
    CHECK(archive->isValid());
    
    // Verify TAR contents
    auto entries = archive->listEntries();
    CHECK(entries.size() >= 3);
    
    bool has_obj = false, has_png = false, has_json = false;
    for (const auto& entry_name : entries) {
        if (entry_name.find("character.obj") != std::string::npos) has_obj = true;
        if (entry_name.find("skin.png") != std::string::npos) has_png = true;
        if (entry_name.find("config.json") != std::string::npos) has_json = true;
    }
    CHECK(has_obj);
    CHECK(has_png);
    CHECK(has_json);
}


TEST(AssetChunking) {
    // Create config space and catalog
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    
    // Set cache directory
    auto cache_dir = file::Path::temp_dir() / FormatString("test_cache_%d", rand());
    catalog->setCacheDir(cache_dir);
    
    // Register codec
    catalog->registerCodecWithPassword("test_chunks", "test_password");
    
    // Create large test file (12MB to trigger chunking)
    auto test_dir = file::Path::temp_dir() / FormatString("test_chunks_%d", rand());
    test_dir.ensureDirectoryExists();
    
    auto large_file = test_dir / "large_test.bin";
    const size_t file_size = 12 * 1024 * 1024; // 12MB
    
    // Create random data
    std::vector<uint8_t> data(file_size);
    for (size_t i = 0; i < file_size; ++i) {
        data[i] = (uint8_t)(rand() % 256);
    }
    
    bool write_success = File::writeBinary(large_file, data);
    CHECK(write_success);
    
    // Create manifest
    auto manifest = AssetCatalog::createManifest(
        catalog,
        "chunk_manifest",
        "1.0",
        "test_chunks",
        test_dir / "manifest.json"
    );
    
    // Create asset
    platform_list_t platforms = {"mac", "linux"};
    assetid_list_t dependencies;
    
    auto asset = AssetManifest::createAsset(
        manifest,
        "large_asset",
        100,
        "binary",
        test_dir.toStdString(),
        platforms,
        dependencies
    );
    
    // Repackage - this should trigger chunking
    asset->repackage();
    
    // Verify asset is chunked
    CHECK(asset->isChunked());
    CHECK(asset->_chunk_manifest != nullptr);
    
    // Verify chunk files were created
    auto chunks_dir = catalog->getChunksDir();
    
    // Should have 3 chunks (12MB / 4MB = 3)
    CHECK(asset->_chunk_manifest->_chunks.size() == 3);
    
    // Verify each chunk file exists
    for (size_t i = 0; i < asset->_chunk_manifest->_chunks.size(); ++i) {
        auto chunk_filename = FormatString("%s.chunk.%04zu.enc", 
                                         asset->_storage_hash.c_str(), i);
        auto chunk_path = chunks_dir / chunk_filename;
        CHECK(chunk_path.doesPathExist());
    }
}

} // anonymous namespace

TEST(AssetCatalogChunking) {
    // Test comprehensive chunking functionality
    auto cfgspc = createTestConfigSpace();
    auto catalog = std::make_shared<AssetCatalog>(cfgspc);
    catalog->registerCodecWithPassword("test_ns", "test_password");
    
    // Set cache directory to avoid default
    auto cache_dir = file::Path::temp_dir() / FormatString("test_cache_%d", rand());
    catalog->setCacheDir(cache_dir);
    
    // Create temp directory for test assets
    auto test_dir = file::Path::temp_dir() / FormatString("test_chunking_%d", rand());
    test_dir.ensureDirectoryExists();
    auto pak_dir = test_dir / "test_pak";
    pak_dir.ensureDirectoryExists();
    
    // Create manifest
    auto manifest = AssetCatalog::createManifest(
        catalog,
        "chunk_test",
        "1.0",
        "test_ns",
        test_dir / "chunk_manifest.json"
    );
    
    // Create large file that will trigger chunking
    std::string large_data;
    size_t data_size = ChunkManifest::chunk_threshold + (2 * 1024 * 1024); // threshold + 2MB
    large_data.reserve(data_size);
    for (size_t i = 0; i < data_size; ++i) {
        large_data += char('A' + (i % 26));
    }
    File::writeString(pak_dir / "large.dat", large_data);
    
    // Create asset with tar_root
    platform_list_t platforms = {"darwin"};
    assetid_list_t deps;
    auto asset = AssetManifest::createAsset(
        manifest,
        "chunked_asset",
        100,
        "asset_pak",
        test_dir.c_str(),
        platforms,
        deps,
        "test_pak"  // tar_root
    );
    
    // Verify chunk manifest was created
    CHECK(asset->_chunk_manifest != nullptr);
    CHECK(asset->_chunk_manifest->_total_size > ChunkManifest::chunk_threshold);
    
    // Verify chunk metadata
    size_t total_chunk_size = 0;
    for (size_t i = 0; i < asset->_chunk_manifest->_chunks.size(); ++i) {
        const auto& chunk = asset->_chunk_manifest->_chunks[i];
        
        // Check chunk offset is correct
        CHECK(chunk._offset == i * ChunkManifest::chunk_size);
        
        // Check chunk size (last chunk may be smaller)
        if (i < asset->_chunk_manifest->_chunks.size() - 1) {
            CHECK(chunk._size == ChunkManifest::chunk_size);
        } else {
            CHECK(chunk._size > 0);
            CHECK(chunk._size <= ChunkManifest::chunk_size);
        }
        
        // Verify hash is set
        CHECK(chunk._hash != 0);
        
        total_chunk_size += chunk._size;
    }
    
    // Verify total size matches
    CHECK(total_chunk_size == asset->_chunk_manifest->_total_size);
    
    // Verify chunk files exist
    auto chunks_dir = catalog->getChunksDir();
    for (size_t i = 0; i < asset->_chunk_manifest->_chunks.size(); ++i) {
        const auto& chunk = asset->_chunk_manifest->_chunks[i];
        auto chunk_path = chunks_dir / FormatString("%llu.chunk.%04zu", chunk._hash, i);
        CHECK(chunk_path.doesPathExist());
        
        // Verify chunk file size
        struct stat st;
        stat(chunk_path.c_str(), &st);
        CHECK(st.st_size == chunk._compressed_size);
    }
    
    // Test chunk manifest serialization
    auto manifest_path = catalog->getEncryptedDir() / (asset->_storage_hash + ".chunkmanifest");
    CHECK(manifest_path.doesPathExist());
    
    // Read and parse manifest JSON
    std::ifstream manifest_file(manifest_path.c_str());
    std::string manifest_json((std::istreambuf_iterator<char>(manifest_file)),
                              std::istreambuf_iterator<char>());
    CHECK(!manifest_json.empty());
    
    // Verify JSON contains expected fields
    CHECK(manifest_json.find("total_size") != std::string::npos);
    CHECK(manifest_json.find("file_hash") != std::string::npos);
    CHECK(manifest_json.find("chunks") != std::string::npos);
    CHECK(manifest_json.find("is_encrypted") != std::string::npos);
    
    // Test chunk assembly
    datablock_list_t chunk_data_blocks;
    for (size_t i = 0; i < asset->_chunk_manifest->_chunks.size(); ++i) {
        const auto& chunk = asset->_chunk_manifest->_chunks[i];
        auto chunk_path = chunks_dir / FormatString("%llu.chunk.%04zu", chunk._hash, i);
        
        // Read chunk data
        File chunk_file(chunk_path, EFM_READ);
        size_t chunk_size = 0;
        chunk_file.GetLength(chunk_size);
        
        auto chunk_block = std::make_shared<DataBlock>();
        chunk_block->reserve(chunk_size);
        chunk_block->_storage.resize(chunk_size);
        chunk_file.Read(const_cast<uint8_t*>(chunk_block->data()), chunk_size);
        
        chunk_data_blocks.push_back(chunk_block);
    }
    
    // Assemble chunks
    ChunkAssembler::Config config;
    ChunkAssembler assembler(asset->_chunk_manifest, config);
    auto assembly_result = assembler.assembleFromChunks(chunk_data_blocks);
    
    CHECK(assembly_result->success);
    CHECK(assembly_result->assembled_data != nullptr);
    CHECK(assembly_result->assembled_data->length() == asset->_chunk_manifest->_total_size);
    
    // Verify assembled data hash matches
    auto xxhasher = std::make_shared<XXH64HASH>();
    xxhasher->init();
    xxhasher->accumulate(assembly_result->assembled_data->data(), 
                        assembly_result->assembled_data->length());
    xxhasher->finish();
    CHECK(xxhasher->result() == asset->_chunk_manifest->_file_hash);
}

TEST(AssetConfigSpaceOperations) {
    // Create config space
    auto cfgspc = std::make_shared<AssetConfigSpace>();
    
    // Create test configs
    auto temp_dir = file::Path::temp_dir() / FormatString("test_configs_%d", rand());
    temp_dir.ensureDirectoryExists();
    
    // Config 1
    auto config1_file = temp_dir / "config1.json";
    std::string config1_json = R"({
        "namespaces": {
            "ns1": {
                "encryption_key": "key1",
                "upload_location": "remote1"
            }
        },
        "locations": {
            "remote1": "https://server1.com/"
        },
        "destinations": {
            "cache": "<assetcache>"
        }
    })";
    File::writeString(config1_file, config1_json);
    
    // Config 2
    auto config2_file = temp_dir / "config2.json";
    std::string config2_json = R"({
        "namespaces": {
            "ns2": {
                "encryption_key": "key2",
                "upload_location": "remote2"
            }
        },
        "locations": {
            "remote2": "https://server2.com/"
        },
        "destinations": {
            "stage": "<stage>"
        }
    })";
    File::writeString(config2_file, config2_json);
    
    // Load configs into space
    auto cfg1 = AssetConfig::loadFromFile(config1_file);
    auto cfg2 = AssetConfig::loadFromFile(config2_file);
    
    CHECK(cfg1 != nullptr);
    CHECK(cfg2 != nullptr);
    
    // Add to config space
    cfgspc->_configs["config1"] = cfg1;
    cfgspc->_configs["config2"] = cfg2;
    cfgspc->_config_paths["config1"] = config1_file;
    cfgspc->_config_paths["config2"] = config2_file;
    
    // Configs are already added, merged() will compute lazy merge
    // No need to manually create merged config
    
    // Test config retrieval
    auto retrieved_cfg1 = cfgspc->getConfig("config1");
    CHECK(retrieved_cfg1 == cfg1);
    
    // Test merged config
    auto merged = cfgspc->merged();
    CHECK(merged != nullptr);
    
    // Verify merged config has both namespaces
    auto ns1_key = merged->getEncryptionKeyForNamespace("ns1");
    auto ns2_key = merged->getEncryptionKeyForNamespace("ns2");
    CHECK(ns1_key == "key1");
    CHECK(ns2_key == "key2");
    
    // Test writeToDisk
    cfgspc->writeToDisk();
    
    // Verify files were written
    CHECK(config1_file.doesPathExist());
    CHECK(config2_file.doesPathExist());
    
    // Test loadFromDisk
    path_list_t paths = {config1_file, config2_file};
    auto loaded_cfgspc = AssetConfigSpace::loadFromDisk(paths);
    CHECK(loaded_cfgspc != nullptr);
    CHECK(loaded_cfgspc->getConfig("config1") != nullptr);
    CHECK(loaded_cfgspc->getConfig("config2") != nullptr);
}
