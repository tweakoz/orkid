# Asset Catalog Python Bindings Usage Audit

This audit tracks usage of each Python binding in `/obt.project/` scripts and modules.
Tests are excluded from this count.

## AssetCatalog Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `__init__(space)` | 2 | assets.py (x2) |
| `instance` (property) | 9 | asset.info.py, config.info.py, fetch.py, location.list.py, manifest.info.py, manifest.list.py, namespace.info.py, namespace.list.py, tree.py |
| `space` (property) | 0 | - |
| `config_space` (property) | 3 | namespace.info.py, tree.py, assets.py |
| `merged_config` (property) | 1 | assets.py |
| `register_namespace()` | 0 | - |
| `find_namespace()` | 1 | namespace.info.py |
| `list_namespaces()` | 4 | manifest.list.py, namespace.list.py, tree.py (x2) |
| `load_manifests_from_path()` | 1 | upload.py |
| `add_manifest()` | 0 | - |
| `get_manifest()` | 5 | asset.info.py, manifest.info.py, manifest.list.py, namespace.info.py (x2) |
| `loadFromGlobalManifests()` (static) | 2 | assets.py (x2) |
| `createManifest()` (static) | 1 | assets.py |
| `registerCodec()` | 0 | - |
| `registerCodecWithPassword()` | 2 | assets.py (x2) |
| `codecForNamespace()` | 0 | - |
| `clearCodecs()` | 0 | - |
| `get()` | 1 | fetch.py |
| `enqueue_get()` | 1 | fetch.py |
| `has_asset()` | 0 | - |
| `get_asset_info()` | 3 | asset.info.py (x2), namespace.info.py |
| `list_assets()` | 6 | fetch.py (x3), namespace.info.py, tree.py (x2) |
| `list_assets_in_namespace()` | 0 | - |
| `all_fqids` (property) | 0 | - |
| `toJson()` | 0 | - |
| `repackage()` | 0 | - |
| `upload()` (backward compat) | 0 | - |
| `uploadNamespace()` | 1 | upload.py |
| `uploadAsset()` | 1 | upload.py |
| `uploadAllNamespaces()` | 1 | upload.py |
| `cache_dir` (property) | 1 | asset.info.py |
| `encrypted_dir` (property) | 0 | - |
| `chunks_dir` (property) | 0 | - |
| `receipts_dir` (property) | 0 | - |
| `temp_dir` (property) | 0 | - |

## AssetManifest Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `namespace` (property) | 4 | asset.info.py, manifest.info.py (x3) |
| `version` (property) | 4 | asset.info.py, manifest.info.py (x3) |
| `assets` (property) | 3 | manifest.info.py (x3) |
| `loadFromFile()` (static) | 1 | manifest.info.py |
| `parse_from_string()` (static) | 0 | - |
| `createAsset()` (static) | 1 | assets.py |
| `toJson()` | 1 | assets.py |
| `getCodec()` | 0 | - |
| `repackage()` | 0 | - |
| `upload()` | 0 | - |

## AssetEntry Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `id` | 0 | - |
| `namespace` (property) | 0 | - |
| `fqid` (property) | 0 | - |
| `type` | 8 | asset.info.py (x2), manifest.info.py (x2), namespace.info.py, assets.py (x3) |
| `priority` | 4 | asset.info.py (x2), manifest.info.py (x2) |
| `merge` | 0 | - |
| `local_loc` | 3 | asset.info.py, manifest.info.py (x2) |
| `relative_path` | 0 | - |
| `_tar_root` | 0 | - |
| `size` | 3 | asset.info.py, namespace.info.py, assets.py |
| `storage_hash` | 6 | asset.info.py (x3), manifest.info.py, namespace.info.py, assets.py |
| `content_hash` | 3 | asset.info.py, assets.py (x2) |
| `hash_algorithm` | 1 | asset.info.py |
| `modification_time` | 0 | - |
| `is_compressed` | 0 | - |
| `is_encrypted` | 1 | namespace.info.py |
| `compressed_size` | 0 | - |
| `compression_type` | 0 | - |
| `platforms` | 5 | asset.info.py (x2), manifest.info.py (x2), namespace.info.py |
| `namespace_id` | 0 | - |
| `chunk_manifest` | 1 | asset.info.py |
| `resolved_local_path` (property) | 1 | namespace.info.py |
| `local_encrypted_path` (property) | 1 | namespace.info.py |
| `is_valid()` | 0 | - |
| `get_validation_error()` | 0 | - |
| `supports_current_platform()` | 0 | - |
| `is_chunked()` | 1 | namespace.info.py |
| `toJson()` | 0 | - |
| `repackage()` | 0 | - |
| `upload()` | 0 | - |
| `filename` | 3 | asset.info.py, manifest.info.py (x2) |
| `remote_loc` | 1 | manifest.info.py |
| `dependencies` | 3 | asset.info.py, manifest.info.py (x2) |
| `is_cached` | 2 | asset.info.py (x2) |

## AssetNamespace Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `__init__(id)` | 0 | - |
| `id` (property) | 0 | - |
| `has_codec()` | 0 | - |

## AssetRequest Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `__init__()` | 0 | - |
| `__init__(namespace)` | 0 | - |
| `__init__(namespace, asset_id)` | 0 | - |
| `namespace` | 0 | - |
| `asset_id` | 0 | - |
| `is_valid()` | 0 | - |
| `set_progress_callback()` | 0 | - |

## AssetFuture Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `asset_id` | 0 | - |
| `is_complete()` | 0 | - |
| `wait()` | 1 | fetch.py |
| `cancel()` | 0 | - |
| `get_result()` | 0 | - |

## AssetResult Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `data` | 0 | - |
| `status` | 0 | - |
| `error_detail` | 2 | fetch.py (x2) |
| `download_time` | 0 | - |
| `processing_time` | 0 | - |
| `bytes_downloaded` | 2 | fetch.py (x2) |
| `is_success()` | 2 | fetch.py (x2) |
| `__bool__` | 0 | - |

## ChunkManifest Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `chunk_size` (static) | 1 | asset.info.py |
| `chunk_threshold` (static) | 0 | - |
| `total_size` | 1 | asset.info.py |
| `file_hash` | 1 | asset.info.py |
| `chunks` | 3 | asset.info.py (x3) |
| `compression` | 1 | asset.info.py |
| `is_encrypted` | 1 | asset.info.py |

## ChunkMeta Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `offset` | 2 | asset.info.py (x2) |
| `size` | 2 | asset.info.py (x2) |
| `compressed_size` | 2 | asset.info.py (x2) |
| `hash` | 2 | asset.info.py (x2) |

## AssetConfigSpace Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `loadGlobalConfigs()` (static) | 1 | assets.py |
| `loadConfigFromDisk()` (static) | 1 | assets.py |
| `merged_config` (property) | 2 | asset.info.py, namespace.info.py |
| `getNamespaceRemoteLocation()` | 2 | asset.info.py, namespace.info.py |

## AssetConfig Class

| Method/Property | Usage Count | Used In |
|-----------------|-------------|---------|
| `loadFromFile()` (static) | 1 | config.info.py |
| `resolveLocalPath()` | 4 | asset.info.py (x2), assets.py (x2) |
| `resolveRemoteLocation()` | 2 | asset.info.py, namespace.info.py |
| `getEncryptionKeyForNamespace()` | 3 | config.info.py (x2), namespace.info.py |
| `getRemoteLocationForNamespace()` | 3 | namespace.info.py, config.info.py (x2) |
| `namespaces` | 1 | config.info.py |
| `destinations` | 2 | config.info.py (x2) |

## Upload Receipt Properties

| Property | Usage Count | Used In |
|----------|-------------|---------|
| `success` | 3 | upload.py (x3) |
| `total_files` | 1 | upload.py |
| `successful_files` | 1 | upload.py |
| `failed_files` | 1 | upload.py |
| `bytes_uploaded` | 1 | upload.py |
| `total_duration` | 1 | upload.py |
| `status_message` | 1 | upload.py |
| `destination` | 1 | upload.py |
| `errors` | 1 | upload.py |

## Enums

| Enum | Usage Count | Used In |
|------|-------------|---------|
| `AssetState` | 0 | - |
| `AssetStatus` | 0 | - |
| `CompressionType` | 0 | - |

## Utility Functions (in assets.py)

| Function | Usage Count | Used In |
|----------|-------------|---------|
| `default_cfg_and_catalog()` | 2 | asset.info.py, upload.py |

## Summary of Unused Bindings

### Completely Unused Classes:
- **AssetRequest** - All methods unused
- **AssetNamespace** - All methods unused

### Completely Unused Enums:
- **AssetState**
- **AssetStatus**
- **CompressionType**

### Heavily Unused Methods:
- **AssetCatalog**: 17 of 35 methods unused (49%)
- **AssetManifest**: 4 of 10 methods unused (40%)
- **AssetEntry**: 15 of 32 properties/methods unused (47%)
- **AssetFuture**: 4 of 5 methods unused (80%)
- **AssetResult**: 5 of 8 properties unused (63%)

### Most Used Bindings:
1. `AssetCatalog.instance` - 9 uses
2. `AssetEntry.type` - 8 uses
3. `AssetEntry.storage_hash` - 6 uses
4. `AssetCatalog.list_assets()` - 6 uses
5. `AssetEntry.platforms` - 5 uses
5. `AssetCatalog.get_manifest()` - 5 uses