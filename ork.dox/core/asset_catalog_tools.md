# Asset Catalog Command-Line Tools Reference

---

## Overview

The Asset Catalog system provides command-line tools for fetching, uploading, and inspecting assets. All tools use the `ork.python` interpreter and read configuration from the `ORKID_ASSET_MANIFEST_DIRS` environment variable.

**Note**: Asset building and packaging is by nature, application specific and handled by namespace-specific tools (e.g., `ork.singularity.data.package.py`). Therefore it is not covered in this document.

---

## Asset Operations

### ork.asset.catalog.fetch.py

Downloads and caches assets from the catalog with support for concurrent fetching.

```bash
# Fetch specific assets by pattern
ork.asset.catalog.fetch.py -p "game|textures|*.dds"

# Fetch all assets in namespace
ork.asset.catalog.fetch.py -n game_textures

# Force re-download
ork.asset.catalog.fetch.py -p "game|config.json" -f

# Disable cache completely
ork.asset.catalog.fetch.py -p "game|level1.pak" --disable-cache

# Parallel downloads (default: 1)
ork.asset.catalog.fetch.py -n game --parallel 4

# Multiple patterns/namespaces
ork.asset.catalog.fetch.py -p "*.pak" -p "*.json" -n audio
```

**Options:**
- `-p, --pattern`: Asset pattern(s) to fetch (can specify multiple)
- `-n, --namespace`: Fetch all assets from namespace(s)
- `-f, --force`: Force download even if cached
- `--disable-cache`: Bypass cache, always download from remote
- `--parallel`: Number of parallel downloads (1=sequential/concurrent, >1=parallel threads)

**Features:**
- Concurrent fetching using `enqueue_get()` API when parallel=1
- Thread pool parallel downloads when parallel>1
- Pattern matching with wildcards
- Progress tracking with download statistics
- Password authentication for protected namespaces

### ork.asset.catalog.upload.py

Uploads packaged assets to configured remote locations.

```bash
# Upload single namespace
ork.asset.catalog.upload.py --namespace game_textures

# Upload all namespaces
ork.asset.catalog.upload.py --all

# Upload single asset
ork.asset.catalog.upload.py --asset "game|player_model"

# Dry run mode
ork.asset.catalog.upload.py --namespace game --dry-run

# Custom manifest directory
ork.asset.catalog.upload.py --namespace game \
  --manifest-dir ${PROJECT_DIR}/manifests
```

**Options:**
- `--namespace`: Upload specific namespace
- `--all`: Upload all namespaces
- `--asset`: Upload single asset by FQID
- `--dry-run`: Show what would be uploaded without uploading
- `--manifest-dir`: Override manifest directory

**Output:**
- Upload receipts with success/failure counts
- Bytes uploaded and transfer duration
- Per-namespace results when using `--all`

---

## Inspection Tools

### ork.asset.catalog.tree.py

Displays hierarchical tree view of all namespaces and assets.

```bash
ork.asset.catalog.tree.py
```

**Example Output:**
```
Asset Catalog Tree:

game/
  ├── textures
  ├── models
  └── audio
singularity/
  └── std

Total: 2 namespaces, 4 assets
```

### ork.asset.catalog.asset.info.py

Shows detailed information about a specific asset.

```bash
# Text output
ork.asset.catalog.asset.info.py "game|textures|player.dds"

# JSON output for scripting
ork.asset.catalog.asset.info.py "game|models|weapon.glb" --json
```

**Information Displayed:**
- Asset type, priority, platforms
- Local and remote locations (resolved)
- File size and hashes (content, storage)
- Cache status and encrypted file location
- Chunk information if applicable
- Dependencies if any

### ork.asset.catalog.namespace.list.py

Lists all available namespaces.

```bash
ork.asset.catalog.namespace.list.py
```

**Output:**
```
Found 3 namespaces:

game
game_textures
singularity
```

### ork.asset.catalog.namespace.info.py

Shows detailed namespace information.

```bash
# Text output
ork.asset.catalog.namespace.info.py game

# JSON output
ork.asset.catalog.namespace.info.py game --json
```

**Information Displayed:**
- Manifest presence and version
- Asset count and list
- Encryption key status
- Upload location configuration

### ork.asset.catalog.manifest.list.py

Lists all manifest files across configured directories.

```bash
ork.asset.catalog.manifest.list.py
```

**Output:**
```
Manifests loaded:

${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests/
  └── std_asset_manifest.json
${PROJECT_DIR}/asset_manifests/
  └── game.json
  └── game_textures.json

Total: 3 manifest files
Namespaces with manifests: game, game_textures, singularity
```

### ork.asset.catalog.manifest.info.py

Shows manifest details for a namespace.

```bash
# Text output
ork.asset.catalog.manifest.info.py game_textures

# JSON output
ork.asset.catalog.manifest.info.py game_textures --json
```

**Information Displayed:**
- Manifest version and file location
- All assets with full details
- Storage hashes and platforms
- Dependencies between assets

---

## Configuration Tools

### ork.asset.catalog.config.list.py

Lists all configuration files and their structure.

```bash
ork.asset.catalog.config.list.py
```

**Output Shows:**
- All loaded config files
- Namespace configurations (keys hidden)
- Location mappings
- Destination templates

### ork.asset.catalog.config.info.py

Shows configuration for specific namespace.

```bash
# Text output
ork.asset.catalog.config.info.py game

# JSON output
ork.asset.catalog.config.info.py game --json
```

### ork.asset.catalog.location.list.py

Lists all configured locations with details.

```bash
ork.asset.catalog.location.list.py
```

**Information Displayed:**
- Remote locations (CDN URLs)
- Local destination paths
- Which namespaces use each location
- Resolution of template variables

---

## Cache Management

### ork.asset.catalog.cache.show.py

Lists files in the cache directory.

```bash
ork.asset.catalog.cache.show.py
```

**Note**: Simply runs `find .` in the cache directory to show all cached files.

### ork.asset.catalog.cache.clear.py

Clears the entire cache directory.

```bash
ork.asset.catalog.cache.clear.py
```

**Warning**: This immediately deletes everything in the cache with `rm -rf`. There is no confirmation prompt or namespace filtering.

---

## Environment Setup

### Required Environment Variables

```bash
# Manifest search paths (colon-separated)
export ORKID_ASSET_MANIFEST_DIRS="${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests:${PROJECT_DIR}/manifests"

# Stage directory for cache
export OBT_STAGE="${HOME}/.obt/stage"

# Encryption keys (optional, can be in configs)
export ORKID_ASSET_KEY_game="secret_key_123"
```

### Directory Structure

```
${ORKID_ASSET_MANIFEST_DIRS}/
  config.json           # Global configuration
  *.json               # Manifest files

${OBT_STAGE}/assetcache/
  enc/                 # Encrypted assets
    {hash}.enc         # Single assets
    chunks/            # Chunked assets
      {hash}.chunk.0
      {hash}.chunk.manifest
  receipts/            # Upload receipts
  temp/                # Temporary files
```

---

## Common Workflows

### Initial Setup

```bash
# 1. Set manifest directories
export ORKID_ASSET_MANIFEST_DIRS="${ORKID_WORKSPACE_DIR}/ork.data/asset_manifests"

# 2. List available assets
ork.asset.catalog.tree.py

# 3. Fetch required assets
ork.asset.catalog.fetch.py -n game --parallel 4
```

### Build and Upload Workflow

```bash
# 1. Run data build (creates assets)
./build_assets.sh --output /tmp/build

# 2. Package assets (namespace-specific tool)
# Note: This example uses a hypothetical packaging tool
# Actual packaging depends on your namespace

# 3. Upload to CDN
ork.asset.catalog.upload.py --namespace my_game

# 4. Verify upload
ork.asset.catalog.asset.info.py "my_game|level_data"
```

### Development Workflow

```bash
# 1. Fetch with cache disabled for testing
ork.asset.catalog.fetch.py -p "game|config.json" --disable-cache

# 2. Inspect asset details
ork.asset.catalog.asset.info.py "game|config.json" --json | jq

# 3. Clear cache if needed
ork.asset.catalog.cache.clear.py --namespace game

# 4. Re-fetch with parallel downloads
ork.asset.catalog.fetch.py -n game --parallel 4
```

### Debugging Issues

```bash
# Check if asset exists
ork.asset.catalog.asset.info.py "game|missing.dat"

# Verify namespace configuration
ork.asset.catalog.namespace.info.py game

# Check manifest
ork.asset.catalog.manifest.info.py game

# Inspect cache
ork.asset.catalog.cache.show.py

# Force re-download
ork.asset.catalog.fetch.py -p "game|broken.dat" --disable-cache
```

---

## Password Authentication

Protected namespaces requiring authentication will prompt for passwords:

```bash
$ ork.asset.catalog.fetch.py -n singularity_ext
Password for cdn.example.com: ****
Fetching singularity_ext assets...
```

Passwords are cached for the session duration. The system detects password-protected namespaces automatically based on API key configuration.

---

## Error Handling

Tools provide detailed error messages:

```bash
# Asset not found
$ ork.asset.catalog.fetch.py -p "invalid|asset"
Asset not found: invalid|asset
Available assets: []

# Network error
$ ork.asset.catalog.fetch.py -p "game|large.pak"
✗ game|large.pak: Connection timeout

# Permission denied
$ ork.asset.catalog.fetch.py -n protected
✗ protected|data: Password authentication required
```

---

## Performance Tips

1. **Use parallel downloads** for multiple assets:
   ```bash
   ork.asset.catalog.fetch.py -n game --parallel 4
   ```

2. **Fetch namespaces** instead of individual assets:
   ```bash
   ork.asset.catalog.fetch.py -n game_textures
   ```

3. **Cache warming** on deployment:
   ```bash
   # Pre-fetch all required assets
   for ns in game audio textures; do
     ork.asset.catalog.fetch.py -n $ns --parallel 4
   done
   ```

4. **Verify before upload** to avoid failures:
   ```bash
   ork.asset.catalog.upload.py --namespace game --dry-run
   ```
