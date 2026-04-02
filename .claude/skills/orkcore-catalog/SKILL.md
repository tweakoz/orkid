---
name: orkcore-catalog
description: Answer questions about orkid's asset catalog system, manifests, asset entries, config spaces, chunked downloads/uploads, encryption codecs, import pipeline, and the catalog_import.py tool. Use when the user asks about asset packaging, manifests, CDN, encryption, or the import/export workflow.
user-invocable: false
---

# Orkid Asset Catalog System Reference

When answering questions about asset management in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| AssetCatalog | `ork.core/inc/ork/asset/catalog/catalog.h` |
| Catalog Impl | `ork.core/src/asset/catalog/catalog.cpp` |
| Manifest Loading | `ork.core/src/asset/catalog/catalog_manifests.cpp` |
| AssetManifest | `ork.core/inc/ork/asset/catalog/manifest.h` |
| AssetEntry | `ork.core/inc/ork/asset/catalog/manifest.h` (same file) |
| AssetConfig / ConfigSpace | `ork.core/inc/ork/asset/catalog/config.h` |
| ChunkManifest | `ork.core/inc/ork/asset/catalog/chunk_manifest.h` |
| ChunkAssembler | `ork.core/inc/ork/asset/catalog/chunk_assembler.h` |
| FetchRequest | `ork.core/inc/ork/asset/catalog/request.h` |
| Namespace | `ork.core/inc/ork/asset/catalog/namespace.h` |
| Encryption | `ork.core/src/asset/catalog/catalog_crypt.cpp` |
| Catalog Impl Detail | `ork.core/src/asset/catalog/catalog_impl.h` |
| Python Bindings | `ork.core/pyext/pyext_asset_catalog.cpp` |
| Config Bindings | `ork.core/pyext/pyext_asset_config.cpp` |
| Import Module | `obt.project/scripts/ork/catalog_import.py` |
| Import CLI | `obt.project/bin/ork.catalog.import.py` |
| Catalog GUI Tool | `obt.project/bin/ork.catalog.tool.py` |
| Python Assets Helper | `obt.project/scripts/ork/assets.py` |

## Architecture Overview

### AssetCatalog (Singleton)
- `globalInstance()` — thread-safe lazy singleton via `std::once_flag`
- `reloadAllManifests(catalog)` — purge and reload from disk
- `loadFromGlobalManifests(catalog)` — load from `ORKID_ASSET_MANIFEST_DIRS`
- `fetch(fqid)` / `fetchAsync(fqid)` — download assets (sync/async)
- `uploadAsset(fqid)` / `uploadNamespace(ns)` — upload to CDN
- `findAssetEntry(fqid)` — lookup metadata without download
- `list_namespaces("*")` / `list_assets("*")` — enumeration with wildcards
- Asset IDs are fully-qualified: `namespace|asset_id`

### Internal State (CatalogImpl)
```
CatalogState (protected by LockedResource):
  _manifests_by_namespace  — map<ns, list<manifest>>
  _nodes_by_namespace      — map<ns, AssetNamespace>
  _entries_by_assetid      — map<fqid, AssetIndexEntry>
  _codecs_by_namespace     — map<ns, EncryptionCodec>
```

### AssetManifest
- `loadFromFile(path)` — parse JSON manifest
- `toJson()` / `saveToFile()` — serialize
- `merge(other)` — merge manifests (other takes priority)
- `createAsset(...)` — builder pattern for new entries
- Contains map of `asset_id -> AssetEntry`

### AssetEntry Metadata
- Identity: `_id`, `_namespace`, `_type` ("asset" or "asset_pak")
- Hashes: `_storage_hash` (CAFS naming), `_content_hash` (verification)
- Sizes: `_archive_size`, `_compressed_size`, `_encrypted_size`
- Location: `_local_loc`, `_tar_root`, `_filters`
- Chunking: `_chunk_manifest` (16MiB chunks for large assets)
- Platform: `_platforms` (["mac", "linux"])

### ConfigSpace / AssetConfig
- Loaded from `config.json` in each manifest directory
- Namespaces map to encryption keys and remote locations
- `LocationInfo`: download_url, upload_url, api keys, TLS settings
- `resolveRemoteLocation(name)` — resolve location by name
- `getEncryptionKeyForNamespace(ns)` — get cipher key

### Chunked Downloads
- Threshold: 16MiB (assets larger get chunked)
- `ChunkManifest`: total_size, file_hash, list of ChunkMeta (offset, size, hash)
- `ChunkAssembler`: reassemble + decrypt + decompress + verify
- `ChunkDisassembler`: split + compress + encrypt

### FetchRequest
- Async result: `wait()`, `isComplete()`, `cancel()`
- Progress: `progress` (0.0-1.0), `bytes_downloaded`, `chunks_completed`
- States: NEW → ENQUEUED → DOWNLOADING → PROCESSING → SUCCEEDED/FAILED

### Import Pipeline (catalog_import.py)
- `ImportConfig` dataclass: namespace, source_dir, local_loc, manifest, assets
- `AssetDefinition`: id, include patterns, exclude patterns
- `AssetImporter.run(upload, dry_run, list_only, verbose)`
- Variable resolution: `<stage>`, `<assetcache>`, `<temp>`, `${ENV_VAR}`
- `export_config(config, path)` — write config JSON

## Environment Variables
- `ORKID_ASSET_MANIFEST_DIRS` — colon-separated manifest directories
- `ORKID_KEY_{NAMESPACE}` — encryption key override per namespace

## How to Answer

1. For catalog API: read `catalog.h` for public methods
2. For manifest format: read `manifest.h` and check JSON examples in manifest dirs
3. For import workflow: read `catalog_import.py`
4. For config structure: read `config.h` and check `config.json` files
5. For chunking: read `chunk_manifest.h` and `chunk_assembler.h`
