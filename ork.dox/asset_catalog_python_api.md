# Asset Catalog Python API Reference

---

## Core Classes - Actually Used in Scripts

### AssetCatalog

Main interface for asset management in Python. This documentation only includes methods actually used in the command-line tools.

#### Initialization

```python
from orkengine import core

# Standard initialization pattern used in all scripts
cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
catalog = core.AssetCatalog(space=cfgspc)
core.AssetCatalog.loadFromGlobalManifests(catalog)
```

#### Asset Retrieval (Used in Scripts)

```python
# Synchronous fetch - used in fetch.py
result = catalog.get(
    asset_id,           # str: Fully qualified asset ID
    decrypt=True,       # bool: Decrypt if encrypted
    disable_cache=False # bool: Bypass cache
)

# Async fetch with future - used in fetch.py
future = catalog.enqueue_get(
    asset_id,
    decrypt=True,
    disable_cache=False
)

# Get metadata - used in asset.info.py, upload.py
asset_info = catalog.get_asset_info("game|models|weapon.glb")
```

#### Properties (Used in Scripts)

```python
# Cache directory - used in asset.info.py
cache_dir = catalog.cache_dir  # str: cache directory path

# Merged config - used in upload.py
merged = catalog.merged_config  # AssetConfig object
```

#### Namespace and Manifest Operations (Used in Scripts)

```python
# List namespaces - used in tree.py, namespace.list.py
namespaces = catalog.list_namespaces("*")

# Find namespace - used in namespace.info.py
ns = catalog.find_namespace("game")

# Get manifest - used in multiple scripts
manifest = catalog.get_manifest("game")

# List assets - used in tree.py, namespace.info.py
all_assets = catalog.list_assets("*")
namespace_assets = catalog.list_assets("game|*")
```

#### Upload Operations (Used in Scripts)

```python
# Upload single namespace - used in upload.py
receipt = catalog.upload("game")

# Upload all namespaces - used in upload.py
receipts = catalog.uploadAllNamespaces()
```

---

### AssetFuture

Async operation handle returned by `enqueue_get()`. Used in fetch.py.

```python
# Start async fetch
future = catalog.enqueue_get("game|level1.pak")

# Wait for completion (blocking) - primary usage in scripts
result = future.wait()

# Access asset ID
print(f"Fetching: {future.asset_id}")
```

---

### AssetResult

Result from asset retrieval operations. Used in fetch.py.

```python
# Check success - primary usage
if result.is_success():
    data = result.data  # bytes
    print(f"Downloaded: {result.bytes_downloaded} bytes")

# Error detail for failed fetches
if not result:
    print(f"Error: {result.error_detail}")
```

---

### AssetEntry

Asset metadata returned by `get_asset_info()`. Used in asset.info.py, namespace.info.py.

```python
# Properties used in scripts
asset_type = entry.type
size = entry.size
storage_hash = entry.storage_hash
content_hash = entry.content_hash
local_loc = entry.local_loc
chunk_manifest = entry.chunk_manifest

# Path resolution
resolved_path = entry.resolved_local_path
encrypted_path = entry.local_encrypted_path

# Check if chunked
is_chunked = entry.is_chunked()
```

---

### AssetManifest

Manifest object returned by `get_manifest()`. Used in namespace.info.py, manifest.info.py.

```python
# Properties used in scripts
namespace = manifest.namespace
version = manifest.version
```

---

### AssetConfigSpace

Configuration management. Used in all scripts for initialization.

```python
# Standard pattern in all scripts
cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
catalog = core.AssetCatalog(space=cfgspc)
```

---

## Enumerations

### AssetStatus

Used for checking result status in scripts.

```python
core.AssetStatus.OK              # Success
core.AssetStatus.NOT_FOUND       # Asset not in catalog
core.AssetStatus.NETWORK         # Network error
core.AssetStatus.PERMISSION      # Access denied
# Other values exist but are not used in scripts
```

---

## Real Usage Examples from Scripts

### Standard Initialization Pattern

```python
from orkengine import core

# Used in all scripts
core.coreappinit()
cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
catalog = core.AssetCatalog(space=cfgspc)
core.AssetCatalog.loadFromGlobalManifests(catalog)

# Do work...

core.coreappexit()
```

### Fetching Assets (from fetch.py)

```python
# Sequential/concurrent fetching with futures
futures = []
for asset_id in asset_ids:
    future = catalog.enqueue_get(asset_id, decrypt=True, disable_cache=disable_cache)
    futures.append((asset_id, future))

# Wait for all
for asset_id, future in futures:
    result = future.wait()
    if result and result.is_success():
        print(f"✓ {asset_id} ({result.bytes_downloaded} bytes)")
    else:
        error = result.error_detail if result else "Unknown error"
        print(f"✗ {asset_id}: {error}")
```

### Listing Assets (from tree.py)

```python
# Get all namespaces and assets
all_namespaces = catalog.list_namespaces("*")
all_assets = catalog.list_assets("*")

# Build tree structure
tree = {}
for fqid in all_assets:
    if '|' in fqid:
        namespace, asset_id = fqid.rsplit('|', 1)
        if namespace not in tree:
            tree[namespace] = []
        tree[namespace].append(asset_id)
```

### Asset Information (from asset.info.py)

```python
# Get detailed asset info
asset_info = catalog.get_asset_info(fqid)
if asset_info:
    print(f"Type: {asset_info.type}")
    print(f"Size: {asset_info.size:,} bytes")
    print(f"Storage hash: {asset_info.storage_hash}")
    
    # Check cache
    cache_dir = catalog.cache_dir
    enc_path = os.path.join(cache_dir, 'enc', f"{asset_info.storage_hash}.enc")
    if os.path.exists(enc_path):
        print("Cached: Yes")
```

### Upload Operations (from upload.py)

```python
# Upload single namespace
receipt = catalog.upload(namespace_id)
if receipt:
    print(f"Success: {receipt.success}")
    print(f"Files: {receipt.successful_files}/{receipt.total_files}")
    print(f"Bytes: {receipt.bytes_uploaded:,}")

# Upload all namespaces
results = catalog.uploadAllNamespaces()
for namespace_id, receipt in results.items():
    if receipt and receipt.success:
        print(f"{namespace_id}: OK")
    else:
        print(f"{namespace_id}: FAILED")
```