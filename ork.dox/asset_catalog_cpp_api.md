# Asset Catalog C++ API Reference

---

## Core Classes

### AssetCatalog

Main interface for asset management operations.

#### Construction

```cpp
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/config.h>

// Create with empty config
auto catalog = std::make_shared<AssetCatalog>();

// Create with config space
auto cfgspc = AssetConfigSpace::loadGlobalConfigs();
auto catalog = std::make_shared<AssetCatalog>(cfgspc);
```

#### Asset Retrieval

```cpp
// Synchronous fetch with decryption
assetresult_ptr_t get(
    const assetid_t& fq_asset_id, 
    bool decrypt = true,
    bool disable_cache = false
);

// Async fetch returning future
assetfuture_ptr_t enqueueGet(
    const assetid_t& fq_asset_id,
    bool decrypt = true, 
    bool disable_cache = false
);

// Check if asset exists
bool hasAsset(const assetid_t& fq_asset_id) const;

// Get metadata without downloading
assetentry_ptr_t getAssetInfo(const assetid_t& fq_asset_id) const;
```

#### Example Usage

```cpp
// Synchronous fetch
auto result = catalog->get("game|textures|player.dds", true, false);
if (result->isSuccess()) {
    auto texture_data = result->_data;
    printf("Downloaded %zu bytes\n", result->_bytes_downloaded);
}

// Async concurrent fetch
auto future1 = catalog->enqueueGet("game|models|weapon.glb");
auto future2 = catalog->enqueueGet("game|audio|music.ogg");

// Wait for both
auto result1 = future1->wait();
auto result2 = future2->wait();
```

#### Namespace Management

```cpp
// Register namespace
void registerNamespace(
    const namespaceid_t& path, 
    assetnamespace_ptr_t ns
);

// Find namespace
assetnamespace_ptr_t findNamespace(
    const namespaceid_t& fq_namespace_id
) const;

// List namespaces with pattern
assetid_list_t listNamespaces(
    const std::string& pattern = "*"
) const;
```

#### Manifest Management

```cpp
// Load manifests from directory
void loadManifestsFromPath(const file::Path& path);

// Add single manifest
void addManifest(assetmanifest_ptr_t manifest);

// Load from environment variable paths
static void loadFromGlobalManifests(assetcatalog_ptr_t self);

// Get manifest for namespace
assetmanifest_ptr_t getManifest(
    const namespaceid_t& namespace_id
) const;
```

#### Codec Management

```cpp
// Register encryption codec
void registerCodec(
    const namespaceid_t& namespace_id,
    encryptioncodec_ptr_t codec
);

// Register with password
void registerCodecWithPassword(
    const namespaceid_t& namespace_id,
    const std::string& password
);

// Get codec for namespace
encryptioncodec_ptr_t codecForNamespace(
    const namespaceid_t& namespace_id
) const;
```

#### Asset Queries

```cpp
// List assets matching pattern
assetid_list_t listAssets(
    const std::string& pattern = "*"
) const;

// List assets in namespace
assetid_list_t listAssetsInNamespace(
    const namespaceid_t& namespace_id
) const;

// Get all asset FQIDs
std::string dumpAllAssetFQIDs() const;
```

#### Upload Operations

```cpp
// Upload single namespace
uploadreceipt_ptr_t upload(
    const namespaceid_t& namespace_id
);

// Upload all namespaces
upload_result_map_t uploadAllNamespaces();
```

#### Cache Management

```cpp
// Set/get cache directory
void setCacheDir(const file::Path& dir);
file::Path getCacheDir() const;

// Get cache subdirectories
file::Path getEncryptedDir() const;    // {cache}/enc
file::Path getChunksDir() const;       // {cache}/enc/chunks  
file::Path getReceiptsDir() const;     // {cache}/receipts
file::Path getTempDir() const;         // {cache}/temp
```

---

### AssetFuture

Represents a pending async asset fetch.

```cpp
struct AssetFuture {
    // Check if complete (non-blocking)
    bool isComplete() const;
    
    // Wait for completion (blocking)
    assetresult_ptr_t wait();
    
    // Cancel the operation
    void cancel();
    
    // Get result if ready (non-blocking)
    assetresult_ptr_t getResult() const;
    
    // Asset being fetched
    assetid_t _asset_id;
};
```

#### Example Usage

```cpp
// Start async fetch
auto future = catalog->enqueueGet("game|level1.pak");

// Poll for completion
while (!future->isComplete()) {
    // Do other work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Or block until ready
auto result = future->wait();
if (result->isSuccess()) {
    // Process data
}
```

---

### AssetResult

Result of an asset retrieval operation.

```cpp
struct AssetResult {
    // Asset data (null for paks)
    datablock_ptr_t _data;
    
    // For asset_pak: extracted files
    std::map<std::string, datablock_ptr_t> _pak_contents;
    
    // Status and error info
    AssetStatus _status;
    std::string _error_detail;
    
    // Performance metrics
    double _download_time;
    double _processing_time;
    size_t _bytes_downloaded;
    
    // Helper methods
    bool isSuccess() const;
    bool isPak() const;
    operator bool() const;
};
```

---

### AssetManifest

Asset manifest management.

```cpp
// Load from file
static assetmanifest_ptr_t loadFromFile(
    const std::string& path
);

// Create asset entry
static assetentry_ptr_t createAsset(
    assetmanifest_ptr_t manifest,
    const assetid_t& id,
    int priority,
    const std::string& type,
    const std::string& local_loc,
    const platform_list_t& platforms,
    const assetid_list_t& dependencies,
    const std::string& tar_root = "",
    const std::vector<std::string>& filters = {}
);

// Repackage all assets
void repackage();

// Upload manifest assets
uploadreceipt_ptr_t upload(
    uploadconfig_ptr_t config,
    const std::string& destination_id
);
```

---

### AssetEntry

Individual asset metadata.

```cpp
struct AssetEntry {
    // Identity
    std::string _id;
    std::string _type;
    int _priority;
    
    // Locations
    std::string _local_loc;
    std::string _relative_path;
    
    // Hash info
    std::string _storage_hash;
    std::string _content_hash;
    size_t _size;
    
    // Chunk info (if chunked)
    chunkmanifest_ptr_t _chunk_manifest;
    
    // Flags
    bool _is_compressed;
    bool _is_encrypted;
    
    // Methods
    bool isValid() const;
    bool isChunked() const;
    file::Path getResolvedLocalPath() const;
    std::string toJson() const;
};
```

---

### AssetConfigSpace

Configuration management.

```cpp
// Load global configs from environment
static assetconfigspace_ptr_t loadGlobalConfigs();

// Load from specific paths
static assetconfigspace_ptr_t loadFromDisk(
    const path_list_t& paths
);

// Create config
assetconfig_ptr_t createConfig(
    const std::string& id,
    const file::Path& path
);

// Get merged config
assetconfig_ptr_t merged() const;

// Write configs to disk
void writeToDisk();
```

---

## Usage Examples

### Basic Asset Loading

```cpp
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/config.h>

// Initialize catalog
auto cfgspc = AssetConfigSpace::loadGlobalConfigs();
auto catalog = std::make_shared<AssetCatalog>(cfgspc);
AssetCatalog::loadFromGlobalManifests(catalog);

// Register codec for namespace
catalog->registerCodecWithPassword("game", "secret_key");

// Fetch asset
auto result = catalog->get("game|textures|player.dds");
if (result->isSuccess()) {
    auto data = result->_data;
    printf("Loaded %zu bytes\n", data->length());
}
```

### Concurrent Asset Loading

```cpp
// Queue multiple assets
std::vector<assetfuture_ptr_t> futures;
std::vector<std::string> asset_ids = {
    "game|models|character.glb",
    "game|textures|skin.dds",
    "game|audio|footsteps.ogg"
};

for (const auto& id : asset_ids) {
    futures.push_back(catalog->enqueueGet(id));
}

// Wait for all to complete
for (size_t i = 0; i < futures.size(); ++i) {
    auto result = futures[i]->wait();
    if (result->isSuccess()) {
        printf("Asset %s: %zu bytes\n", 
               asset_ids[i].c_str(), 
               result->_bytes_downloaded);
    }
}
```

### Handling Asset Paks

```cpp
// Fetch pak with extracted contents
auto result = catalog->get("game|level_data.pak");
if (result->isPak()) {
    // Iterate extracted files
    for (const auto& [path, data] : result->_pak_contents) {
        printf("File: %s (%zu bytes)\n", 
               path.c_str(), data->length());
    }
}
```

### Cache Control

```cpp
// Force download, bypassing cache
auto result = catalog->get("game|config.json", 
                          true,  // decrypt
                          true); // disable_cache

// Set custom cache directory
catalog->setCacheDir(file::Path("/custom/cache"));
```

### Upload Operations

```cpp
// Upload single namespace
auto receipt = catalog->upload("game");
if (receipt->success) {
    printf("Uploaded %zu files, %zu bytes\n",
           receipt->successful_files,
           receipt->bytes_uploaded);
}

// Upload all namespaces
auto receipts = catalog->uploadAllNamespaces();
for (const auto& [ns, receipt] : receipts) {
    printf("Namespace %s: %s\n", 
           ns.c_str(),
           receipt->success ? "OK" : "FAILED");
}
```

### Pattern Matching

```cpp
// List all texture assets
auto textures = catalog->listAssets("*|textures|*");

// List all assets in game namespace
auto game_assets = catalog->listAssetsInNamespace("game");

// Find all pak files
auto paks = catalog->listAssets("*|*.pak");
```

### Error Handling

```cpp
auto result = catalog->get("game|missing.dat");
if (!result->isSuccess()) {
    switch (result->_status) {
        case AssetStatus::NOT_FOUND:
            printf("Asset not in catalog\n");
            break;
        case AssetStatus::PERMISSION:
            printf("Access denied: %s\n", 
                   result->_error_detail.c_str());
            break;
        case AssetStatus::NETWORK:
            printf("Network error: %s\n",
                   result->_error_detail.c_str());
            break;
        default:
            printf("Error: %s\n", 
                   result->_error_detail.c_str());
    }
}
```

### Building and Packaging Assets

```cpp
// Create manifest
auto manifest = AssetCatalog::createManifest(
    catalog,
    "my_assets",           // id
    "1.0.0",              // version  
    "game",               // namespace
    file::Path("manifest.json")
);

// Create asset entry
auto asset = AssetManifest::createAsset(
    manifest,
    "player_model",        // id
    100,                  // priority
    "asset_pak",          // type
    "<cache>/models",     // local_loc
    {"mac", "linux"},     // platforms
    {}                    // dependencies
);

// Package from local files
asset->repackage();

// Upload to CDN
manifest->upload(config, "production");
```