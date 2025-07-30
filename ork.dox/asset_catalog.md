# Asset Catalog System

---

### Summary

The Asset Catalog provides a unified, thread-safe system for managing downloadable content with sophisticated caching, encryption, and state management. It serves dual purposes with symmetric operations:

1. **Build-time**: Packages, encrypts, and uploads assets to CDN
2. **Runtime**: Downloads, decrypts, verifies, caches, and serves assets to applications

The system maintains perfect symmetry:
- **Build**: File → Package → Encrypt → Upload to CDN
- **Runtime**: Download from CDN → Decrypt → Verify → Serve to App

This symmetry ensures that what goes up comes down intact, with end-to-end verification. Built around a flyweight pattern for efficient memory usage and atomic operations for thread safety.

---

### Features

- **Unified Asset Management**
  - Single interface for all asset operations across namespaces
  - Flyweight pattern for AssetRequests with reference counting
  - Atomic state tracking throughout asset lifecycle
  - Thread-safe operations via LockedResource pattern

- **Namespace Organization**
  - Hierarchical namespace structure for asset organization
  - Per-namespace encryption keys and codecs
  - Namespace-specific configuration and policies
  - Pattern-based asset discovery and filtering

- **Robust Download Management**
  - Concurrent download limiting with configurable bounds
  - Progress tracking and error reporting
  - Retry logic with exponential backoff
  - Chunk-based assembly for large assets

- **Security and Integrity**
  - Per-namespace encryption with pluggable codecs
  - Hash verification (MD5, SHA256) with corruption detection
  - Secure key management and codec registration
  - Tamper-resistant asset validation

- **Python and C++ APIs**
  - Full feature parity across both languages
  - Shared pointer management for safe cross-language usage
  - Comprehensive test coverage with TDD methodology

---

### Architecture

```
┌───────────────────────────────────────────────────────────────────┐
│                         AssetCatalog                              │
│  ┌─────────────────┐  ┌──────────────┐  ┌────────────────────┐    │
│  │ ConfigSpace     │  │   Manifests  │  │  AssetRequests     │    │
│  │                 │  │              │  │   (Flyweight)      │    │
│  │ ┌─────────────┐ │  │ ┌──────────┐ │  │ ┌────────────────┐ │    │
│  │ │Config 1     │ │  │ │Manifest 1│ │  │ │Request Pool    │ │    │
│  │ │Config 2     │ │  │ │Manifest 2│ │  │ │                │ │    │
│  │ │Merged Config│ │  │ │Manifest N│ │  │ │Shared State    │ │    │
│  │ └─────────────┘ │  │ └──────────┘ │  │ └────────────────┘ │    │
│  └─────────────────┘  └──────────────┘  └────────────────────┘    │
│                               │                                   │
│  ┌─────────────────────────────────────────────────────────────┐  │
│  │                    Cache System                             │  │
│  │  ┌────────────┐  ┌────────────┐  ┌─────────────────────┐    │  │
│  │  │Encrypted   │  │  Chunks    │  │    Receipts         │    │  │
│  │  │Directory   │  │  Directory │  │    Directory        │    │  │
│  │  │{hash}.enc  │  │            │  │                     │    │  │
│  │  └────────────┘  └────────────┘  └─────────────────────┘    │  │
│  └─────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────────┘
                              ▼
            ┌─────────────────────────────────────┐
            │        Download/Upload System       │
            │   ┌────────────┐  ┌─────────────┐   │
            │   │   HTTPS    │  │    SCP      │   │
            │   │  Uploader  │  │  Uploader   │   │
            │   └────────────┘  └─────────────┘   │
            │   ┌────────────┐  ┌─────────────┐   │
            │   │   HTTPS    │  │     S3      │   │
            │   │  Download  │  │  Download   │   │
            │   └────────────┘  └─────────────┘   │
            └─────────────────────────────────────┘
```

---

### Design Description

The Asset Catalog implements a modern, scalable approach to content management with several key design principles:

**Flyweight Pattern for Memory Efficiency**: AssetRequests use flyweight pattern where identical asset IDs share the same request object, reducing memory overhead and enabling efficient reference counting.

**Atomic State Management**: All state transitions use atomic operations, ensuring thread-safe access without locks during read operations. The nine-state lifecycle provides granular visibility into asset processing.

**Pimpl Pattern**: Public interfaces use private-implementation (pimpl) pattern, hiding implementation details and enabling ABI stability.

**Namespace-Based Organization**: Assets are organized into namespaces with independent encryption keys, policies, and manifests, enabling fine-grained access control.

---

### When to Use

**Large-Scale Content Distribution**: When your application needs to download and cache significant amounts of content from remote servers with efficient local storage.

**Multi-Tenant Applications**: When different content requires different access controls, encryption keys, or download policies organized by namespace.

**Background Asset Loading**: When assets should be downloaded and cached transparently without blocking the main application thread.

**Content Versioning**: When assets have versions, dependencies, and need integrity verification with hash checking.

**Cross-Platform Content**: When the same content system needs to work across different platforms with consistent behavior.

**Development vs Production**: When you need different asset sources (local files vs CDN) with the same API interface.

### Data Build Integration

The Asset Catalog system is designed to work with a separate data build process. The system provides symmetric build-time and runtime operations:

**Build-Time Responsibilities:**

The data build operation is responsible for:
1. **Generating asset files** - Creating textures, models, audio files, etc.
2. **Outputting to local locations** - Placing files at the paths specified by `local_loc` in manifests
3. **Directory structure** - Creating the expected directory hierarchy for asset_paks

The Asset Catalog then:
1. **Packages** - Creates assets with `createAsset()` or paks with `packFromLocal()`
2. **Encrypts** - Calls `repackage()` to compute hashes and encrypt
3. **Uploads** - Sends encrypted assets to CDN via `upload()`

**Runtime Responsibilities:**

At runtime, the Asset Catalog:
1. **Downloads** - Fetches encrypted assets from CDN via `download()` or `get()`
2. **Decrypts** - Uses namespace keys to decrypt assets
3. **Verifies** - Checks content_hash matches original
5. **Serves** - Provides decrypted assets to the application

This separation of concerns allows the build system to focus on asset generation while the catalog handles the symmetric operations of packaging/encryption/distribution (build-time) and retrieval/decryption/serving (runtime).

---

### API Usage Examples

#### C++ Basic Usage

```cpp
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/config.h>

using namespace ork::asset::catalog;

// Create configuration space
auto cfgspc = std::make_shared<AssetConfigSpace>();

// Load configurations from disk
path_list_t config_paths = {
    "assets/config1.json",
    "assets/config2.json"
};
auto loaded_cfgspc = AssetConfigSpace::loadFromDisk(config_paths);

// Create catalog with configuration space
auto catalog = std::make_shared<AssetCatalog>(loaded_cfgspc);

// Load and add manifests
auto manifest = AssetManifest::loadFromFile("assets/game_manifest.json");
catalog->addManifest(manifest);

// Register encryption for namespace
catalog->registerCodecWithPassword("game", "secret_key_123");

// Request assets
auto texture_req = catalog->mergeAssetReq("game|textures|player.dds");
auto model_req = catalog->mergeAssetReq("game|models|weapon.glb");

// Check state
if (texture_req->_state.load() == AssetState::CACHED_MEMORY) {
    auto result = catalog->get("game|textures|player.dds", true); // with decryption
    if (result->status == AssetStatus::OK) {
        auto texture_data = result->data;
        // Use texture data...
    }
}

// Batch retrieval
std::vector<std::string> asset_ids = {
    "game|audio|music.ogg",
    "game|audio|sfx.wav"
};
auto results = catalog->getMany(asset_ids, true);

// Query system state
auto downloading = catalog->getRequestsInState(AssetState::DOWNLOADING);
auto cache_size = catalog->memoryCacheSize();
auto active_downloads = catalog->activeDownloads();
```

#### C++ Builder Pattern API

```cpp
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/config.h>

using namespace ork::asset::catalog;

// Create configuration space
auto cfgspc = std::make_shared<AssetConfigSpace>();

// Load configurations from files
auto cfg1 = AssetConfig::loadFromFile("data/cdntest/config.json");
auto cfg2 = AssetConfig::loadFromFile("data/cdntest2/config.json");

// Add to config space
cfgspc->_configs["test1"] = cfg1;
cfgspc->_configs["test2"] = cfg2;
cfgspc->_config_paths["test1"] = file::Path("data/cdntest/config.json");
cfgspc->_config_paths["test2"] = file::Path("data/cdntest2/config.json");

// Create merged config
cfgspc->_merged = std::make_shared<AssetConfig>();
cfgspc->_merged->merge(*cfg1);
cfgspc->_merged->merge(*cfg2);

// Write configurations to disk
cfgspc->writeToDisk();

// Create catalog with configuration space
auto cat = std::make_shared<AssetCatalog>(cfgspc);

// Create manifest (static factory pattern)
auto m1 = AssetCatalog::createManifest(
    cat,                              // parent catalog
    "cdntest",                        // id
    "1.0.0",                          // version
    "cdntest",                        // namespace
    file::Path("data/cdntest/catalog.json") // file
);

// Create asset (static factory pattern)
auto a1 = AssetManifest::createAsset(
    m1,                               // parent manifest
    "user_id",                        // id
    100,                              // priority
    "text",                           // type
    "<cdntest_remote>/my_remote_asset_dir", // remote
    "<cache>/my_local_asset_dir",     // local
    "my_asset_file.txt",              // filename
    {"mac", "linux"},                 // platforms
    {}                                // dependencies
);

// Asset is automatically packaged when created
// a1 now has content_hash and storage_hash computed
// IMPORTANT: Files must exist at local_loc before creating assets
// The data build process is responsible for outputting files there
// The createAsset call will compute content_hash from the existing file

// Repackage operations
a1->repackage();  // Repackage single asset
m1->repackage();  // Repackage all assets in manifest
cat->repackage(); // Repackage all manifests in catalog

// Upload operations
a1->upload();     // Upload single asset to CDN
m1->upload();     // Upload all assets in manifest
cat->upload();    // Upload all assets in catalog

// Save and load catalog
cat->writeToDisk();  // Write all manifests to disk
cat->download();     // Download all assets from CDN
```

#### C++ Advanced Usage

```cpp
// Namespace management
auto game_ns = std::make_shared<AssetNamespace>("game|levels");
game_ns->setDisplayName("Game Levels");
game_ns->setDescription("Level geometry and lighting data");
game_ns->setCacheEnabled(true);
game_ns->setCacheTTL(7200); // 2 hours
game_ns->setPriority(100);
catalog->registerNamespace("game|levels", game_ns);

// Pattern-based discovery
auto level_assets = catalog->listAssets("game|levels|*");
auto texture_assets = catalog->listAssets("*|textures|*");

// Asset monitoring
auto all_requests = catalog->getRequestsInState(AssetState::NOT_AVAILABLE);
for (auto req : all_requests) {
    float progress = req->_progress.load();
    size_t downloaded = req->_bytes_downloaded.load();
    size_t total = req->_bytes_total.load();
    std::string error;
    req->_error_message.atomicOp([&](const std::string& msg) {
        error = msg;
    });
}

```

#### Python Usage

```python
from orkengine import core

# Create configuration space and catalog
cfgspc = core.AssetConfigSpace()
cfg = cfgspc.createConfig("test", "data/cdntest/config.json")
catalog = core.AssetCatalog(space=cfgspc)

# Or create with empty config space
catalog = core.AssetCatalog()

# Load manifest and register codec  
manifest = core.AssetManifest.loadFromFile("assets/game_manifest.json")
catalog.addManifest(manifest)
catalog.registerCodecWithPassword("game", "secret_key_123")

# Get asset information and retrieve
texture_info = catalog.get_asset_info("game|textures|player.dds")
if texture_info:
    print(f"Size: {texture_info.size}, Hash: {texture_info.content_hash}")

result = catalog.get("game|textures|player.dds", decrypt=True)
if result.status == core.AssetStatus.OK:
    texture_data = result.data
    print(f"Loaded {len(texture_data)} bytes")

# Batch operations
asset_ids = ["game|audio|music.ogg", "game|audio|sfx.wav"]
results = catalog.getMany(asset_ids, decrypt=True)

# Query and monitoring
all_assets = catalog.listAssets("game|*")
namespaces = catalog.listNamespaces("game|*")
all_fqids = catalog.all_fqids  # Property access in Python

# Namespace operations
game_ns = catalog.findNamespace("game|ui")
if game_ns:
    game_ns.setDisplayName("Game UI Assets")
    game_ns.setCacheEnabled(True)

# Upload operations (namespace-specific)
receipt = catalog.upload("game")  # Upload single namespace
receipts = catalog.uploadAllNamespaces()  # Upload all namespaces
```

---

### JSON Schema Documentation

The Asset Catalog system uses two primary JSON file formats for configuration and asset manifests.

#### AssetConfig JSON Schema

The configuration file defines global settings, namespace encryption keys, and location mappings.

```json
{
  "namespaces": {
    "game": {
      "encryption_key": "game_secret_key_123",
      "upload_location": "devcdn"
    },
    "editor": {
      "encryption_key": "editor_key_456",
      "upload_location": "prodcdn"
    }
  },
  "locations": {
    "orkid_std": "https://www.tweakoz.com/resources",
    "devcdn": "https://cdn.example.com/dev",
    "prodcdn": "https://cdn.example.com/prod"
  },
  "destinations": {
    "cache": "<assetcache>",
    "stage": "<stage>",
    "temp": "<temp>"
  }
}
```

**Configuration Schema Fields**:

- `namespaces` (object, required) - Namespace configurations with encryption keys and upload locations
  - Key: namespace identifier (string)
  - Value: namespace configuration object
    - `encryption_key` (string, required) - Encryption key for this namespace
    - `upload_location` (string, required) - Location identifier for uploads

- `locations` (object, required) - Defines named source locations for asset downloads
  - Key: location identifier used in manifests (string) 
  - Value: URL or file path (string)
  - Supported protocols: `https://`, `http://`, `file://`

- `destinations` (object, required) - Maps destination aliases to actual paths
  - Key: destination alias used in manifests (string)
  - Value: path template with variables (string)
  - Variables: `<stage>`, `<temp>`, `<assetcache>`, or literal paths

#### AssetManifest JSON Schema

Each namespace has its own manifest file describing available assets.

```json
{
  "namespace": "game",
  "version": "1.0.0",
  "assets": {
    "textures": {
      "type": "asset_pak",
      "platforms": ["mac", "linux"],
      "priority": 100,
      "local_loc": "<cache>/game",
      "remote_loc": "<devcdn>",
      "filename": "game_textures.tar.xz.enc",
      "storage_hash": "a1b2c3d4e5f67890abcdef1234567890",
      "hash_algorithm": "md5",
      "dependencies": {}
    },
    "models": {
      "type": "asset_pak",
      "platforms": ["mac", "linux"],
      "priority": 90,
      "local_loc": "<cache>/game",
      "remote_loc": "<devcdn>",
      "filename": "game_models.tar.xz.enc",
      "storage_hash": "9876543210fedcba0987654321098765",
      "hash_algorithm": "md5",
      "dependencies": {"textures": ""}
    }
  }
}
```

**Manifest Schema Fields**:

- `namespace` (string, required) - Unique identifier for this asset namespace

- `version` (string, required) - Semantic version of this manifest (e.g., "1.0.0")

- `assets` (object, required) - Map of asset definitions
  - Key: unique asset identifier within namespace (string)
  - Value: asset definition object

**Asset Definition Fields**:

- `type` (string, required) - Asset type identifier
  - Common values: `"asset_pak"`, `"asset"`

- `platforms` (array of strings, required) - Supported platforms
  - Common values: `"mac"`, `"linux"`

- `priority` (integer, required) - Download priority (lower numbers = higher priority)
  - Range: typically 0-1000, where 1000 is lowest priority

- `local_loc` (string, required) - Local location path template where assets are extracted/cached
  - Uses destination aliases from config (e.g., `"<cache>/game"`)

- `remote_loc` (string, required) - Remote location identifier for downloading
  - Must match a key in config's `locations` object, or use template like `"<devcdn>"`

- `filename` (string, required) - Filename at the source location

- `storage_hash` (string, required) - Hash of the encrypted file for content-addressable storage
  - Used to construct filenames: `{storage_hash}.enc`
  - This is the hash of the encrypted data, not the original content
  - Files are stored using content-addressable naming: `{remote_loc}/{storage_hash}.enc`
  - 32-character hexadecimal string for MD5

- `hash_algorithm` (string, optional) - Hash algorithm used
  - Values: `"md5"` (default), `"sha256"`
  - If not specified, defaults to `"md5"` 
- `dependencies` (object, required) - Map of asset dependencies
  - Key: fully qualified asset ID this asset depends on (string)
  - Value: version constraint or empty string (string)
  - Dependencies are downloaded before this asset
  - Use `{}` for no dependencies

**Path Templates and Variables**:

Both configuration destinations and manifest paths support template variables:

- `<stage>` - Orkid stage directory (platform-specific application directory)
- `<temp>` - System temporary directory
- Custom variables from config destinations (e.g., `<cache>`)

**Example Complete Manifest**:

```json
{
  "namespace": "game|characters",
  "version": "2.1.0",
  "assets": {
    "player_base": {
      "type": "asset_pak",
      "platforms": ["mac", "linux"],
      "priority": 500,
      "local_loc": "<cache>/characters",
      "remote_loc": "<prodcdn>",
      "filename": "player_base_v2.tar.xz.enc",
      "storage_hash": "a1b2c3d4e5f67890abcdef1234567890",
      "hash_algorithm": "md5",
      "dependencies": {}
    },
    "player_animations": {
      "type": "asset_pak", 
      "platforms": ["mac", "linux"],
      "priority": 400,
      "local_loc": "<cache>/characters",
      "remote_loc": "<prodcdn>",
      "filename": "player_anims_v2.tar.xz.enc",
      "storage_hash": "9876543210fedcba0987654321098765",
      "hash_algorithm": "md5",
      "dependencies": {"player_base": ""}
    },
    "npc_pack": {
      "type": "asset_pak",
      "platforms": ["mac", "linux"],
      "priority": 200,
      "local_loc": "<cache>/characters",
      "remote_loc": "<devcdn>",
      "filename": "npc_collection.tar.xz.enc", 
      "storage_hash": "fedcba9876543210abcdef0123456789",
      "hash_algorithm": "md5",
      "dependencies": {"player_base": "", "player_animations": ""}
    }
  }
}
```

---

### Upload Operations

The Asset Catalog provides namespace-aware upload operations that prevent cross-namespace contamination. Each namespace has its own configuration for remote locations and encryption keys.

#### Upload API Methods

**AssetEntry::upload()** - Uploads a single asset file to its namespace's configured remote location
```cpp
bool success = asset->upload();
```

**AssetManifest::upload()** - Uploads all assets in a manifest to their configured remote location
```cpp
auto receipt = manifest->upload();
if (receipt->success) {
    printf("Uploaded %zu files successfully\n", receipt->successful_files);
}
```

**AssetCatalog::upload(namespace_id)** - Uploads all manifests for a specific namespace
```cpp
auto receipt = catalog->upload("game|levels");
```

**AssetCatalog::uploadAllNamespaces()** - Uploads all namespaces to their respective configured locations
```cpp
auto receipts = catalog->uploadAllNamespaces();
for (const auto& [namespace_id, receipt] : receipts) {
    printf("Namespace %s: %s\n", namespace_id.c_str(), 
           receipt->success ? "success" : "failed");
}
```

#### Upload Configuration

Upload destinations are configured per-namespace in the AssetConfig:

```json
{
  "namespaces": {
    "game": {
      "encryption_key": "game_key_123",
      "upload_location": "game_cdn"
    },
    "editor": {
      "encryption_key": "editor_key_456",
      "upload_location": "editor_cdn"
    }
  },
  "locations": {
    "game_cdn": "https://cdn.example.com/game",
    "editor_cdn": "https://cdn.example.com/editor"
  }
}
```

The upload system automatically:
- Creates the appropriate uploader based on URL scheme (HTTPS, SCP, S3)
- Uses namespace-specific encryption keys
- Generates upload receipts with detailed transfer statistics
- Handles chunked uploads for large files
- Provides progress callbacks for monitoring

---

### Definitions

**AssetCatalog** - Central coordinator managing all asset operations, caching, and namespace organization

**AssetManifest** - JSON-based descriptor containing asset metadata, dependencies, and download information for a namespace

**AssetNamespace** - Logical grouping identified by namespace ID in manifests and configurations

**AssetRequest** - Flyweight object tracking state, progress, and metadata for a specific asset download/retrieval

**AssetState** - Enumeration tracking asset lifecycle states:
  - NOT_AVAILABLE - Asset not in catalog
  - AVAILABLE - Asset in catalog but not downloaded
  - DOWNLOAD_REQUESTED - Download queued
  - DOWNLOADING - Currently downloading
  - DOWNLOAD_COMPLETE - Downloaded but not processed
  - PROCESSING - Decrypting/decompressing
  - CACHED_DISK - Available on disk
  - CACHED_MEMORY - Loaded in memory cache
  - FAILED - Download or processing failed

**AssetConfig** - Global configuration defining namespace keys, CDN locations, and destination mappings

**LockedResource** - Thread-safe wrapper providing atomic operations on shared data without explicit mutex management

**EncryptionCodec** - Pluggable encryption/decryption interface supporting per-namespace security policies

---

### Asset Paks and Chunking

The Asset Catalog system provides two key features for efficient content management: Asset Paks for bundling related files, and Chunking for handling large files.

#### Asset Paks

Asset paks are TAR archives that bundle entire directory structures into a single file. They are identified by `type = "asset_pak"` in the manifest.

**Important**: The data build operation is responsible for outputting files to the local locations specified in the catalog. The asset catalog system expects files to already exist at their `_local_loc` paths before packaging operations like `repackage()` or `packFromLocal()` are called.

**Key Operations:**

1. **`packFromLocal(fq_asset_id)`**: 
   - Takes a fully-qualified asset ID (e.g., "namespace|asset_name")
   - Looks up the asset entry and verifies it's type "asset_pak"
   - Resolves the asset's `_local_loc` to an actual filesystem path
   - Checks if `{_local_loc}/{_filename}` already exists (caching)
   - If not cached, creates a TAR archive of the ENTIRE directory at `_local_loc`
   - Returns raw TAR data in memory (does not save to disk)
   - Preserves full directory structure and all files recursively

2. **`unpackToLocal(fq_asset_id)`**:
   - Loads the TAR file from `{_local_loc}/{_filename}`
   - Extracts ALL contents to the `_local_loc` directory
   - Preserves directory structure from the TAR
   - Used after downloading a pak from CDN

3. **`extractPakContents(fq_asset_id)`**:
   - Lists files in a TAR without extracting
   - Returns vector of filenames/paths within the archive
   - Used for inspection/validation

**Pak Workflow:**
1. Directory exists at `/path/to/assets/models/` with many files
2. Manifest entry: `local="<cache>/models"`, `filename="models.tar"`, `type="asset_pak"`
3. `packFromLocal()` creates TAR of entire directory
4. `repackage()` encrypts the TAR and computes storage_hash
5. `upload()` sends encrypted TAR to CDN
6. Later: `download()` fetches encrypted TAR
7. `unpackToLocal()` extracts all files back to original structure

#### Chunking

Chunking splits large files into smaller pieces for parallel transfer and reliability.

**Triggering:**
- Files > 10MB are automatically chunked
- Chunk size: 4MB default (configurable via PackagerConfig)
- Applies to ANY file type, including asset_paks

**How it works:**

1. **During `repackage()`**:
   ```cpp
   if (_size > 10 * 1024 * 1024) {  // 10MB threshold
       _chunk_manifest = std::make_shared<ChunkManifest>();
       _chunk_manifest->chunk_size = 4 * 1024 * 1024;  // 4MB chunks
       
       size_t num_chunks = (_size + chunk_size - 1) / chunk_size;
       
       for each chunk:
           - Read chunk_size bytes from file
           - Encrypt chunk data
           - Compute chunk content_hash (hash of raw data)
           - Compute chunk storage_hash (hash of encrypted data)
           - Store encrypted chunk with filename = storage_hash
   }
   ```

2. **Storage Structure**:
   - Each chunk stored as separate file named by its storage_hash
   - Original file's storage_hash = hash of concatenated chunk hashes
   - ChunkManifest tracks: chunk_size, total_chunks, each chunk's index/offset/size/hashes

3. **Download/Assembly**:
   - ChunkDownloadCoordinator manages parallel downloads
   - Each chunk downloaded to temp location
   - Each chunk's storage_hash verified against manifest
   - ChunkAssembler concatenates chunks in order
   - Decrypts assembled file
   - Verifies final content_hash matches original

**Example with Large Asset Pak:**
```
50MB models.tar → 
  Chunk 0: 0-4MB    → encrypted → storage_hash_0
  Chunk 1: 4-8MB    → encrypted → storage_hash_1
  ...
  Chunk 12: 48-50MB → encrypted → storage_hash_12
  
models.tar storage_hash = hash(storage_hash_0 + ... + storage_hash_12)
```

#### Encryption Flow

1. **Small files (< 10MB)**:
   - Read entire file → compute content_hash
   - Encrypt with libsodium → compute storage_hash of encrypted data
   - Store single encrypted file

2. **Large files (>= 10MB)**:
   - Read file in 4MB chunks
   - For each chunk: encrypt → compute chunk storage_hash
   - File's storage_hash = hash of all chunk storage_hashes
   - Store each encrypted chunk separately

3. **Verification**:
   - Download verifies storage_hash (encrypted data integrity)
   - After decryption, verifies content_hash (original data integrity)
   - Ensures CDN tampering is detected

The system provides:
- Efficient parallel chunk transfers
- Resume capability (re-download only failed chunks)
- End-to-end encryption with verification
- Transparent handling (API users just see complete files)
- Support for huge files (games often have multi-GB paks)

---

### Command Line Tools

The Asset Catalog system provides command-line tools for packaging and uploading assets:

#### ork.asset.catalog.package.py

Packages (encrypts/compresses) assets and adds them to catalog manifests.

**Usage:**
```bash
ork.asset.catalog.package.py \
  --namespace your_namespace \
  --output ${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/your_manifest.json \
  --asset-id your_asset_id \
  --priority 100 \
  --remote-loc "<orkid_cdn>/your_namespace" \
  --local-loc "<assetcache>/your_namespace" \
  --asset-pak ${SOME_DATA_FOLDER} \
  --strip-leading
```

**Example - Packaging terrain textures from a data build:**
```bash
ork.asset.catalog.package.py \
  --namespace game_textures \
  --output ${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/game_textures.json \
  --asset-id terrain_pack \
  --priority 100 \
  --remote-loc "<orkid_cdn>/game_textures" \
  --local-loc "<assetcache>/game_textures" \
  --asset-pak /tmp/terrain_textures_build \
  --strip-leading
```

This command will:
1. Create a tar.xz archive from `${SOME_DATA_FOLDER}`
2. Encrypt it with the namespace's encryption key
3. Copy the encrypted file to the asset cache (`${OBT_STAGE}/assetcache/enc/`)
4. Create/update the manifest at `ork.data/asset_manifests/your_manifest.json`

**Key Options:**
- `--strip-leading` - Removes the base directory from tar archive paths
- `--filter "*.png"` - Only include files matching the pattern (for --asset-pak)
- `--platforms mac linux` - Specify target platforms (defaults to current platform)
- `--key YOUR_KEY` - Override the encryption key (otherwise uses environment or config)
- `--asset FILE` - Package a single file instead of a directory (mutually exclusive with --asset-pak)
- `--dependencies ns1|asset1 ns2|asset2` - Specify asset dependencies

#### ork.asset.catalog.upload.py

Uploads packaged assets from the catalog to configured remote locations.

**Usage:**
```bash
# Upload a single namespace
ork.asset.catalog.upload.py --namespace your_namespace

# Upload all namespaces
ork.asset.catalog.upload.py --all

# Upload a single asset
ork.asset.catalog.upload.py --asset "namespace|asset_id"
```

**Examples:**
```bash
# Upload all assets in the game_textures namespace
ork.asset.catalog.upload.py --namespace game_textures

# Dry run to see what would be uploaded
ork.asset.catalog.upload.py --namespace game_textures --dry-run

# Upload with custom manifest directory
ork.asset.catalog.upload.py --namespace game_textures --manifest-dir ${ORKID_WORKSPACE_DIR}/custom_manifests
```

The upload tool will:
1. Load configurations from `ORKID_ASSET_MANIFEST_DIRS`
2. Find all manifests for the specified namespace
3. Upload encrypted assets to the configured CDN location
4. Provide detailed upload receipts with statistics

**Workflow Example:**

1. Run your data build process to generate assets:
   ```bash
   ./build_terrain_textures.sh --output /tmp/terrain_build
   ```

2. Package the built assets:
   ```bash
   ork.asset.catalog.package.py \
     --namespace terrain \
     --output ${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/terrain.json \
     --asset-id textures_v2 \
     --priority 100 \
     --remote-loc "<game_cdn>/terrain" \
     --local-loc "<assetcache>/terrain" \
     --asset-pak /tmp/terrain_build \
     --strip-leading
   ```

3. Upload to CDN:
   ```bash
   ork.asset.catalog.upload.py --namespace terrain
   ```

#### ork.asset.catalog.list.py

Lists all namespaces and assets in a hierarchical tree view across all configured manifest directories.

**Usage:**
```bash
ork.asset.catalog.list.py
```

**Example Output:**
```
Asset Catalog Tree:

YYY
  ├── a
  ├── b
  └── c
XXX
singularity
  └── std

Total: 3 namespaces, 4 assets
```

The list tool will:
1. Load all manifests from directories in `ORKID_ASSET_MANIFEST_DIRS`
2. Display namespaces and their assets in a tree structure
3. Show empty namespaces (like `XXX` in the example)
4. Sort namespaces and assets alphabetically
5. Provide a summary count of namespaces and assets

**Environment Setup:**
```bash
export ORKID_ASSET_MANIFEST_DIRS="${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests:${PROJECT_DIR}/asset_manifests"
```

#### ork.asset.catalog.asset.info.py

Displays detailed information about a specific asset.

**Usage:**
```bash
ork.asset.catalog.asset.info.py "namespace|asset_id" [--json]
```

**Examples:**
```bash
# Get detailed info about an asset
ork.asset.catalog.asset.info.py "singularity|std"

# Check a specific asset's status
ork.asset.catalog.asset.info.py "game_textures|terrain_pack"
```

**Example Output (Text):**
```
Asset: singularity|std
  Type: asset_pak
  Priority: 100
  Platforms: mac, linux
  Local location: <assetcache>/singularity
  Remote location: <orkid_cdn>/singularity
  Filename: singularity_std.tar.xz.enc
  Size: 1,234,567 bytes
  Content hash: fedcba9876543210abcdef0123456789
  Storage hash: a1b2c3d4e5f67890abcdef1234567890
  Hash algorithm: md5
  Cached: Yes
  Manifest namespace: singularity
  Manifest version: 1.0.0
```

**Example Output (JSON):**
```json
{
  "asset_id": "singularity|std",
  "type": "asset_pak",
  "priority": 100,
  "platforms": ["mac", "linux"],
  "local_loc": "<assetcache>/singularity",
  "remote_loc": "<orkid_cdn>/singularity",
  "filename": "singularity_std.tar.xz.enc",
  "size": 1234567,
  "content_hash": "fedcba9876543210abcdef0123456789",
  "storage_hash": "a1b2c3d4e5f67890abcdef1234567890",
  "hash_algorithm": "md5",
  "cached": true,
  "manifest": {
    "namespace": "singularity",
    "version": "1.0.0"
  }
}
```

The assetinfo tool will:
1. Load the asset metadata from the catalog
2. Display comprehensive information including hashes, locations, and dependencies
3. Show caching status and manifest details
4. Optionally output in JSON format for scripting

#### ork.asset.catalog.fetch.py

Downloads and caches a specific asset from the catalog.

**Usage:**
```bash
ork.asset.catalog.fetch.py -p "namespace|asset_id" [-f]
```

**Examples:**
```bash
# Fetch a specific asset
ork.asset.catalog.fetch.py -p "singularity|std"

# Force re-download even if cached
ork.asset.catalog.fetch.py -p "game_textures|terrain_pack" -f

```

**Example Output:**
```
Found asset: singularity|std
Asset info: <AssetEntry: std, type=asset_pak, size=45678>
✓ Successfully fetched singularity|std
  Downloaded: 45,678 bytes
  Download time: 1.23s
  Processing time: 0.45s
```

The fetch tool will:
1. Check if the asset exists in the catalog
2. Download the encrypted asset from the CDN if not cached
3. Decrypt and verify the asset
4. Store in the local cache at `${OBT_STAGE}/assetcache/`
5. Report download statistics

**Error Handling:**
```bash
# If asset doesn't exist
ork.asset.catalog.fetch.py -p "invalid|asset"
# Output: Asset not found: invalid|asset
# Output: Available assets: []

# If download fails
ork.asset.catalog.fetch.py -p "broken|asset"
# Output: ✗ Failed to fetch broken|asset
# Output:   Error: Connection timeout
# Output:   Status: FAILED
```

#### ork.asset.catalog.location.list.py

Lists all configured locations (both remote CDN locations and local destination paths) with their details.

**Usage:**
```bash
ork.asset.catalog.location.list.py
```

**Example Output:**
```
Configured Locations:

Remote Locations:
  orkid_std:
    URL: https://www.tweakoz.com/resources
    Type: HTTPS
    Source: ${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/config.json
    Used by namespaces: singularity

  game_cdn:
    URL: https://cdn.example.com/game
    SCP Destination: cdn-server:~/game_assets
    API Key: ******** (hidden)
    Certificate Check: Disabled
    Type: HTTPS (Complex)
    Source: ${ORKID_WORKSPACE_DIR}/game/asset_manifests/config.json
    Used by namespaces: game, game_textures

Local Destinations:
  <assetcache>:
    Template: <stage>/assetcache
    Resolves to: ${OBT_STAGE}/assetcache
    Exists: Yes
    Contents: 47 items
    Source: ${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/config.json

  <shared>:
    Template: <stage>/share
    Resolves to: ${OBT_STAGE}/share
    Exists: Yes
    Contents: 24 items
    Source: ${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/config.json

Total: 2 remote locations, 2 local destinations
```

The location list tool will:
1. Load all configurations from `ORKID_ASSET_MANIFEST_DIRS`
2. Display remote locations with:
   - URL or complex configuration details
   - Type (HTTPS, SCP, S3, etc.)
   - Which config file defines it
   - Which namespaces use it for uploads
3. Display local destinations with:
   - Template pattern
   - Resolved actual path
   - Whether the path exists
   - Number of items if it's a directory
   - Which config file defines it
4. Provide a summary count

#### ork.asset.catalog.namespace.list.py

Lists all available namespaces in the catalog.

**Usage:**
```bash
ork.asset.catalog.namespace.list.py
```

**Example Output:**
```
Found 3 namespaces:

ZZZ
XXX
singularity
```

#### ork.asset.catalog.namespace.info.py

Shows detailed information about a specific namespace.

**Usage:**
```bash
ork.asset.catalog.namespace.info.py namespace_id [--json]
```

**Example Output:**
```
Namespace: singularity
  Has manifest: Yes
  Manifest version: 1.0.0
  Asset count: 1
  Assets:
    - std
  Has config: Yes
    Encryption key: ******** (hidden)
    Upload location: orkid_std
```

#### ork.asset.catalog.manifest.list.py

Lists all manifest files across configured directories.

**Usage:**
```bash
ork.asset.catalog.manifest.list.py
```

**Example Output:**
```
Manifests loaded:

${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/
  └── std_asset_manifest.json
${ORKID_WORKSPACE_DIR}/game/asset_manifests/
  └── game.json
  └── game_textures.json

Total: 3 manifest files

Namespaces with manifests: game, game_textures, singularity
```

#### ork.asset.catalog.manifest.info.py

Shows detailed information about a manifest for a specific namespace.

**Usage:**
```bash
ork.asset.catalog.manifest.info.py namespace_id [--json]
```

**Example Output:**
```
Manifest for namespace: game_textures
  Version: 1.0.0
  File: ${ORKID_WORKSPACE_DIR}/game/asset_manifests/game_textures.json
  Asset count: 2

Assets:

  terrain_pack:
    Type: asset_pak
    Priority: 100
    Platforms: mac, linux
    Filename: terrain_pack.tar.xz.enc
    Storage hash: a1b2c3d4e5f67890abcdef1234567890
    Local location: <assetcache>/game_textures
    Remote location: <game_cdn>/game_textures

  character_pack:
    Type: asset_pak
    Priority: 90
    Platforms: mac, linux
    Filename: character_pack.tar.xz.enc
    Storage hash: 9876543210fedcba0987654321098765
    Local location: <assetcache>/game_textures
    Remote location: <game_cdn>/game_textures
    Dependencies: terrain_pack
```

#### ork.asset.catalog.config.list.py

Lists all configuration files and shows their data topology.

**Usage:**
```bash
ork.asset.catalog.config.list.py
```

**Example Output:**
```
Configuration files loaded:

${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/config.json
${ORKID_WORKSPACE_DIR}/game/asset_manifests/config.json

Total: 2 config files

Configuration data topology:

${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/config.json:
  namespaces:
    singularity:
      encryption_key: ******** (hidden)
      upload_location: orkid_std
  locations:
    orkid_std: https://www.tweakoz.com/resources
  destinations:
    assetcache: <stage>/assetcache
    shared: <stage>/share
    stage: <stage>

${ORKID_WORKSPACE_DIR}/game/asset_manifests/config.json:
  namespaces:
    game:
      encryption_key: ******** (hidden)
      upload_location: game_cdn
    game_textures:
      encryption_key: ******** (hidden)
      upload_location: game_cdn
  locations:
    game_cdn: https://cdn.example.com/game
```

#### ork.asset.catalog.config.info.py

Shows configuration details for a specific namespace.

**Usage:**
```bash
ork.asset.catalog.config.info.py namespace_id [--json]
```

**Example Output:**
```
Configuration for namespace: game_textures
  Source file: ${ORKID_WORKSPACE_DIR}/game/asset_manifests/config.json
  Encryption key: ******** (hidden)
  Upload location: game_cdn

Available destination variables:
  <assetcache>: ${OBT_STAGE}/assetcache
  <shared>: ${OBT_STAGE}/share
  <stage>: ${OBT_STAGE}
```

**Complete Workflow Example:**

```bash
# 1. Set up manifest directories
export ORKID_ASSET_MANIFEST_DIRS="${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests"

# 2. List available assets
ork.asset.catalog.list.py

# 3. Package new assets from a build
ork.asset.catalog.package.py \
  --namespace my_game \
  --output ${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/my_game.json \
  --asset-id level_data \
  --priority 50 \
  --remote-loc "<game_cdn>/my_game" \
  --local-loc "<assetcache>/my_game" \
  --asset-pak ${BUILD_OUTPUT}/levels \
  --strip-leading

# 4. Upload to CDN
ork.asset.catalog.upload.py --namespace my_game

# 5. On another machine, fetch the assets
ork.asset.catalog.fetch.py -p "my_game|level_data"

# 6. Verify it's in the list
ork.asset.catalog.list.py | grep my_game

# 7. Get detailed info about the asset
ork.asset.catalog.asset.info.py "my_game|level_data"

# 8. Check cache status with JSON output
ork.asset.catalog.asset.info.py "my_game|level_data" --json | jq .cached
```