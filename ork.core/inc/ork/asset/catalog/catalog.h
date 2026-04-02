////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/kernel/datablock.h>
#include <ork/util/crypt.h>
#include <ork/util/download_manager.h>
#include <ork/util/upload_manager.h>
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/namespace.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/mutex.h>
#include <functional>
#include <atomic>
#include <regex>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////////////////////
// Main asset catalog class - Central orchestrator for all asset operations
//
// The AssetCatalog is the heart of the asset management system, providing:
// 1. Hierarchical namespace management (e.g., game|textures|characters)
// 2. Manifest collection and priority-based resolution
// 3. Asset discovery, retrieval, and caching (memory + disk tiers)
// 4. Encryption/decryption with per-namespace codec inheritance
// 5. Compression/decompression (LZ4/LZ4HC) with automatic detection
// 6. Chunked download support for large assets with parallel fetching
// 7. Progress tracking and cancellation for long operations
// 8. Integration with CDN/HTTP/file sources via content-addressable URLs
//
// Key design principles:
// - Codec reuse: Create encryption codec once per namespace, reuse for session
// - Lazy loading: Only fetch/decrypt/decompress when actually needed
// - Cache awareness: Check memory cache, then disk cache, then download
// - Thread safety: All public methods are thread-safe via generation-based versioning
// - Extensibility: Easy to add new compression algorithms, codecs, or sources
// - Generation versioning: Catalog state changes increment generation counter
//   ensuring cache coherency and safe concurrent access
////////////////////////////////////////////////////////////////////////////////

struct AssetCatalog {
  ////////////////////////////////////////////////////////////////////////////////
  // Construction/Destruction
  ////////////////////////////////////////////////////////////////////////////////
  AssetCatalog();
  explicit AssetCatalog(assetconfigspace_ptr_t space);
  ~AssetCatalog();
  
  // Global instance - thread-safe lazy initialization
  static assetcatalog_ptr_t globalInstance();
  
  assetfqid_ptr_t findAsset(const assetid_t& fq_asset_id) const;

  ////////////////////////////////////////////////////////////////////////////////
  // === Namespace Management ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Register a namespace (creates hierarchy automatically)
  void registerNamespace(const namespaceid_t& path, assetnamespace_ptr_t ns);
  
  // Find namespace by full path
  assetnamespace_ptr_t findNamespace(const namespaceid_t& fq_namespace_id) const;
  
  // List namespaces matching pattern (supports wildcards)
  assetid_list_t listNamespaces(const std::string& pattern = "*") const;
    
  ////////////////////////////////////////////////////////////////////////////////
  // === Manifest Management ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Load manifests from specific directory
  void loadManifestsFromPath(const file::Path& path);
    
  // Add a single manifest
  // Typically called when manifest JSON arrives via network request
  // Increments generation and creates new versioned state
  // Old state remains valid for in-flight operations
  void _addManifest(assetmanifest_ptr_t manifest);
  
  // Create a new manifest with builder pattern
  static assetmanifest_ptr_t createManifest(
    assetcatalog_ptr_t catalog,
    const std::string& id,
    const std::string& version,
    const namespaceid_t& namespace_id,
    const file::Path& file);
  
  // Get manifest for namespace (returns first if multiple)
  manifest_list_t manifestsForNamespace(const namespaceid_t& namespace_id) const;
    
  // Set manifest search paths
  void setManifestSearchPaths(const path_list_t& paths);
  
  // Load manifests from ORKID_ASSET_MANIFEST_DIRS environment variable
  // This method:
  // 1. Reads ORKID_ASSET_MANIFEST_DIRS (colon-separated paths)
  // 2. Loads config.json from each directory into config space
  // 3. Loads all non-config JSON files as manifests
  // 4. Registers codecs for all namespaces found in configs
  static void loadFromGlobalManifests(assetcatalog_ptr_t self);

  // Purge all loaded manifests, asset index, and namespace tree,
  // then reload everything from disk via loadFromGlobalManifests.
  static void reloadAllManifests(assetcatalog_ptr_t self);
  
  ////////////////////////////////////////////////////////////////////////////////
  // === Codec Management ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Register codec for a namespace
  void registerCodec(const namespaceid_t& namespace_id, encryptioncodec_ptr_t codec);
  
  // Register codec by password (creates codec internally)
  void registerCodecWithPassword(const namespaceid_t& namespace_id, const std::string& password);
  
  // Get codec for namespace (with inheritance)
  encryptioncodec_ptr_t codecForNamespace(const namespaceid_t& namespace_id) const;
  
  // Clear all codecs
  void clearCodecs();
  
  ////////////////////////////////////////////////////////////////////////////////
  // === Asset Retrieval ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Synchronous asset retrieval
  // Returns AssetResult with error code if failed
  // FATAL: OrkAsserts internally on AssetFatalError conditions
  // NOTE: Mixed error handling is intentional - see types.h for policy
  //
  // Typical usage pattern for regular assets:
  // 1. Receiver (e.g., GameObject) needs asset: "game|ui|button|icon.png"
  // 2. Calls catalog->get(asset_id) 
  // 3. Receives fully assembled datablock (chunks transparent to caller)
  // 4. Uses result->data for texture loading, mesh creation, etc.
  //
  // For asset_pak types:
  // 1. Downloads {hash}.enc from _remote_loc
  // 2. Saves to <assetcache>/enc/{filename}
  // 3. Decrypts → LZ4 decompresses → Untars to _local_loc
  // 4. Loads each extracted file into memory
  // 5. Returns AssetResult with pak_contents map populated, data = nullptr
  //
  // Generation safety is built-in - if manifest changes during retrieval,
  // the operation either completes with old version or retries with new
  fetchrequest_ptr_t fetch(
    const assetid_t& fq_asset_id,
    bool enable_cache = true,
    chunk_completed_callback_t on_chunk_completed = nullptr);

  // Async version - enqueue asset fetch and return future immediately
  // Allows parallel fetching of multiple assets
  fetchrequest_ptr_t fetchAsync(
    const assetid_t& fq_asset_id,
    bool enable_cache = true,
    chunk_completed_callback_t on_chunk_completed = nullptr);
  
  // Invalidate a cached fetch request so it can be re-fetched
  void invalidateRequest(const assetid_t& fq_asset_id);

  // Check if asset exists without downloading
  bool hasAsset(const assetid_t& fq_asset_id) const;
  
  // Get asset metadata without downloading
  assetindexentry_ptr_t findAssetIndexEntry(const assetid_t& fq_asset_id) const;
  assetentry_ptr_t findAssetEntry(const assetid_t& fq_asset_id) const;
  assetindexentry_ptr_t _addNewAssetIndexEntry(const assetid_t& fq_asset_id);
  
  ////////////////////////////////////////////////////////////////////////////////
  // === Asset Pak Operations ===
  ////////////////////////////////////////////////////////////////////////////////
  
  
  // Create asset pak from local directory structure  
  // Uses manifest's _local_loc to determine source files
  // Returns AssetResult with pak data and status
  // Scans directory for files matching manifest entries
  // Example: creates pak from files in /assets/models/ directory
  datablock_ptr_t _packFromLocal(assetfqid_ptr_t fqid);
  
  ////////////////////////////////////////////////////////////////////////////////
  // === Asset Queries ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // List assets matching pattern (supports wildcards and regex)
  assetid_list_t listAssets(const std::string& pattern = "*") const;
  
  // List assets in a specific namespace
  assetid_list_t listAssetsInNamespace(const namespaceid_t& namespace_id) const;
  
  // Dump all asset FQIDs via recursive descent of namespace tree (root at top, 1 per line)
  std::string dumpAllAssetFQIDs() const;
      
  ////////////////////////////////////////////////////////////////////////////////
  // === Configuration ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Set asset config space
  void setConfigSpace(assetconfigspace_ptr_t space);
  
  // Get current config space
  assetconfigspace_ptr_t getConfigSpace() const;
  
  // Set download manager
  void setDownloadManager(downloadmanager_ptr_t mgr);
  downloadmanager_ptr_t getDownloadManager() const;
  
  ////////////////////////////////////////////////////////////////////////////////
  // === URL Generation (Single Source of Truth) ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Upload URLs
  URL getAssetUploadURL(const AssetEntry* entry, locationinfo_ptr_t location) const;
  URL getAssetDownloadURL(const AssetEntry* entry, locationinfo_ptr_t location) const;

  ////////////////////////////////////////////////////////////////////////////////

  std::string getChunkFilename(std::string basename, //
                               size_t chunk_index ) const;  //

  
  ////////////////////////////////////////////////////////////////////////////////
  // === Serialization ===
  ////////////////////////////////////////////////////////////////////////////////
      
  // Build fully qualified asset ID
  // Format: namespace_id + "|" + asset_path
  // Example: buildAssetId("game|ui|button", "icon.png") -> "game|ui|button|icon.png"
  static assetid_t buildAssetId(const namespaceid_t& namespace_id, const std::string& asset_path);
    
  // Repackage all assets in all manifests
  void repackage();
  
  // Upload a single namespace to its configured remote location
  // Returns: upload receipt for the namespace
  uploadreceipt_ptr_t uploadNamespace(
    const namespaceid_t& namespace_id,
    asset_completed_callback_t on_asset_completed = nullptr);

  // Upload a single asset to its configured remote location
  // Returns: upload receipt for the asset
  uploadreceipt_ptr_t uploadAsset(
    const assetid_t& fq_asset_id,
    chunk_completed_callback_t on_chunk_completed = nullptr);

  // Upload all namespaces to their configured remote locations
  // Returns: map of namespace ID to upload receipt
  upload_result_map_t uploadAllNamespaces(
    namespace_completed_callback_t on_namespace_completed = nullptr);
  
  // Convert wildcard pattern to regex (utility function)
  static std::regex wildcardToRegex(const std::string& pattern);
  
  ////////////////////////////////////////////////////////////////////////////////
  // === Cache Directory Management ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Set/get cache directory (defaults to <stage>/assetcache)
  void setCacheDir(const file::Path& dir);
  file::Path getCacheDir() const;
  
  // Get standard subdirectories
  file::Path getEncryptedDir() const;    // {_cache_dir}/enc
  file::Path getChunksDir() const;       // {_cache_dir}/enc/chunks
  file::Path getReceiptsDir() const;     // {_cache_dir}/receipts
  file::Path getTempDir() const;         // {_cache_dir}/temp
  
  ////////////////////////////////////////////////////////////////////////////////
  // === Asset Request State Management (Flyweight) ===
  ////////////////////////////////////////////////////////////////////////////////
  
  // Get flyweight request for an asset (creates if doesn't exist)
  fetchrequest_ptr_t _mergeRequest(assetfqid_ptr_t fqid);
  
  // Get flyweight namespace for an ID (creates if doesn't exist)
  assetnamespace_ptr_t _mergeNamespace(const namespaceid_t& namespace_id);
        
// Members:
  ////////////////////////////////////////////////////////////////////////////////
  // === Generation-Based Versioning System ===
  //
  // All catalog state is versioned with a monotonic generation counter.
  // This solves three key problems:
  // 1. Thread safety - single RW lock with clear semantics
  // 2. Cache coherency - entries tagged with generation, auto-invalid on change  
  // 3. Manifest reload races - in-flight ops know which version they're using
  //
  // When manifests change (reload, add, remove):
  // - Generation increments
  // - New VersionedState created with copy-on-write
  // - Old state remains valid for in-flight operations
  // - Cache entries from old generations ignored
  ////////////////////////////////////////////////////////////////////////////////
  
  // Implementation pointer
  svar64_t _impl;

  // Cache directory for all asset operations
  file::Path _cache_dir;

  ////////////////////////////////////////////////////////////////////////////////
  // === Shutdown ===
  ////////////////////////////////////////////////////////////////////////////////

  // Signal shutdown to all in-flight operations
  void requestShutdown();

  // Wait for all in-flight operations to complete
  void drainPendingOperations();
};


} // namespace ork::asset::catalog