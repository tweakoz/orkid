#include <ork/asset/catalog/uploader.h>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////
// FetchRequest - Encapsulates all parameters for asset fetching
////////////////////////////////////////////////////////////////

struct FetchRequest {
  assetid_t asset_id;
  AssetLocation location;
  assetentry_ptr_t asset_info;
  bool decrypt = true;
  bool disable_cache = false;
  // Future expansion: priority, timeout, retry_count, etc.
};

////////////////////////////////////////////////////////////////
// CatalogImpl - Implementation class for AssetCatalog
////////////////////////////////////////////////////////////////

struct CatalogImpl {
  ////////////////////////////////////////////////////////////////////////////////
  // Internal Types - defined first for VersionedState
  ////////////////////////////////////////////////////////////////////////////////
  
  // Type aliases for LockedResource usage
  using download_progress_map_t = std::map<assetid_t, DownloadProgress>;
  using chunk_coordinator_map_t = std::map<assetid_t, chunkdownloadcoordinator_ptr_t>;
  using chunk_upload_coordinator_map_t = std::map<assetid_t, chunkuploadcoordinator_ptr_t>;
  
  // Asset index entry
  struct AssetIndexEntry {
    namespaceid_t namespace_id;
    std::string asset_path;
    assetmanifest_ptr_t manifest;
    assetentry_ptr_t entry;
  };
  
  
  // Statistics struct
  struct Stats {
    size_t total_assets = 0;
    size_t total_namespaces = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    size_t bytes_downloaded = 0;
    size_t bytes_served_from_cache = 0;
    double total_download_time = 0;
    double total_processing_time = 0;
    
    // Calculate cache hit rate
    float getCacheHitRate() const;
    
    // Get average download speed
    double getAverageDownloadSpeed() const;
  };

  // All mutable catalog state - thread-safe via LockedResource
  // No longer versioned - direct mutation with proper locking
  struct CatalogState {
    std::map<namespaceid_t, manifest_list_t> _manifests_by_namespace;
    std::unordered_map<namespaceid_t, assetnamespace_ptr_t> _nodes_by_namespace;
    std::map<assetid_t, AssetIndexEntry> _entries_by_assetid;
    std::map<namespaceid_t, encryptioncodec_ptr_t> _codecs_by_namespace;
  };

  CatalogImpl(AssetCatalog* catalog);
  
  AssetCatalog* _catalog;
    
  // All catalog state - thread-safe via LockedResource direct mutation
  LockedResource<CatalogState> _state;
  
  // Configuration
  assetconfigspace_ptr_t _config_space;
  path_list_t _manifest_search_paths;
  
  // Namespace management - tree structure remains outside versioned state
  // as it's used for structural navigation
  assetnamespace_ptr_t _root_namespace;
  
  // Flyweight asset request storage
  LockedResource<std::map<assetid_t, assetreq_ptr_t>> _active_requests;
  
  // Download/Upload management
  // Managers follow singleton pattern - live until program exit
  // No explicit shutdown needed, same as opq.cpp::concurrentQueue()
  downloadmanager_ptr_t _download_manager;
  uploadmanager_ptr_t _upload_manager;
  std::atomic<bool> _shutdown{false};
  
  // Progress tracking - thread-safe via LockedResource
  LockedResource<download_progress_map_t> _downloads_by_assetid;
  LockedResource<chunk_coordinator_map_t> _coordinators_by_assetid;
  LockedResource<chunk_upload_coordinator_map_t> _upload_coordinators_by_assetid;
  
  // Statistics - thread-safe via LockedResource
  mutable LockedResource<Stats> _stats;
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // Internal Methods moved from header
  ////////////////////////////////////////////////////////////////////////////////
  
  
  // Locate asset in manifests
  assetlocation_ptr_t locateAsset(const std::string& fq_asset_id) const;
  
  // Atomic file download - just gets bytes from a URL
  datablock_ptr_t downloadFile(const URL& url, const locationinfo_ptr_t& location_info = nullptr);
  
  // High-level asset retrieval (new refactored method)
  assetresult_ptr_t getAsset(fetchrequest_ptr_t request);
  
  // Download phases
  datablock_ptr_t downloadAssetData(fetchrequest_ptr_t request);
  datablock_ptr_t downloadChunkedData(fetchrequest_ptr_t request);
  datablock_ptr_t downloadSingleData(fetchrequest_ptr_t request);
  
  // Cache helpers
  file::Path getCachePathForAsset(const AssetLocation& location) const;
  file::Path getCachePathForChunk(const AssetLocation& location, size_t chunk_index) const;
  bool verifyCachedFileHash(const file::Path& cache_path, const std::string& expected_hash) const;
  bool verifyCachedChunkHash(const file::Path& cache_path, chunk_hash_t expected_hash) const;
  datablock_ptr_t readCachedFile(const file::Path& cache_path) const;
  bool saveToCacheFile(const datablock_ptr_t& data, const file::Path& cache_path) const;
  
  // Processing phases
  datablock_ptr_t processAssetData(
    datablock_ptr_t data,
    fetchrequest_ptr_t request
  );
  datablock_ptr_t decryptData(
    datablock_ptr_t data,
    const namespaceid_t& namespace_id
  );
  datablock_ptr_t decompressData(
    datablock_ptr_t data,
    CompressionType compression_type
  );
  
  // Asset type handlers (everything is a pak now)
  void handleAssetPak(datablock_ptr_t data, AssetResult& result, fetchrequest_ptr_t request);
  void writeAssetPakToLocal(const assetentry_ptr_t& asset_info, AssetResult& result);
    
  
  // Parse fully qualified asset ID into namespace and asset path
  std::pair<std::string, std::string> parseAssetId(const std::string& fq_asset_id) const;
  
  // Convert wildcard pattern to regex
  static std::regex wildcardToRegex(const std::string& pattern);


};

  ////////////////////////////////////////////////////////////////////////////////
// Chunk assembly coordination moved from header
//
// Tracks the state of a multi-chunk download and assembly operation:
// - Downloads can complete out of order
// - Assembly only happens when all chunks are ready
// - Temporary files cleaned up after assembly
// - Progress reported per-chunk and overall
// - Works with ChunkManifest to know the expected chunks
////////////////////////////////////////////////////////////////////////////////

  struct ChunkDownloadCoordinator {
  // Type aliases
  using path_vect_t = path_list_t;
  
  // Identity
  std::string asset_id;
  AssetLocation location;
  chunkmanifest_ptr_t chunk_manifest;
  
  // Progress tracking
  std::atomic<int> chunks_downloaded{0};     // Incremented atomically as chunks complete
  std::atomic<int> chunks_failed{0};         // Track failed chunks for retry
  const int total_chunks;                    // Total expected (from chunk_manifest)
  
  // State management
  std::atomic<bool> assembly_triggered{false}; // Ensure assembly happens once
  std::atomic<bool> cancelled{false};
  
  // Storage
  file::Path temp_directory;                 // Where chunks are stored
  LockedResource<path_vect_t> chunk_paths;  // Path for each chunk (indexed) - thread-safe
  
  // Callbacks - using pysafe types for Python GIL compatibility
  pysafe_completion_callback_t on_complete;
  pysafe_error_callback_t on_error;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Constructor
  ////////////////////////////////////////////////////////////////////////////////
  ChunkDownloadCoordinator(const std::string& id, 
                           const AssetLocation& loc,
                           chunkmanifest_ptr_t manifest)
      : asset_id(id), location(loc), chunk_manifest(manifest), 
        total_chunks(manifest ? manifest->_chunks.size() : 0) {
    // Initialize chunk paths vector
    chunk_paths.atomicOp([this](path_vect_t& paths) {
      paths.resize(total_chunks);
    });
  }
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // Cancel the download
  ////////////////////////////////////////////////////////////////////////////////
  void cancel() {
    cancelled = true;
    // TODO: Cancel pending downloads and cleanup
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress calculation
  ////////////////////////////////////////////////////////////////////////////////
  float getOverallProgress() const {
    return static_cast<float>(chunks_downloaded.load()) / total_chunks;
  }
  
  bool isComplete() const {
    return chunks_downloaded.load() == total_chunks;
  }
  
  bool hasFailed() const {
    return chunks_failed.load() > 0 && !isComplete();
  }
};

////////////////////////////////////////////////////////////////
// ChunkUploadCoordinator - Manages parallel upload of file chunks
////////////////////////////////////////////////////////////////

struct ChunkUploadCoordinator {
  // Type aliases
  using upload_list_t = std::vector<upload_ptr_t>;
  
  // Identity
  std::string asset_id;
  std::string storage_hash;
  chunkmanifest_ptr_t chunk_manifest;
  
  // Progress tracking
  std::atomic<int> chunks_uploaded{0};     // Incremented atomically as chunks complete
  std::atomic<int> chunks_failed{0};         // Track failed chunks for retry
  const int total_chunks;                    // Total expected (from chunk_manifest)
  
  // State management
  std::atomic<bool> completed{false};        // Ensure completion happens once
  std::atomic<bool> all_success{true};       // Track if all uploads succeeded
  
  // Storage
  file::Path chunks_directory;               // Where chunks are stored locally
  LockedResource<upload_list_t> active_uploads;  // Track active upload objects
  
  // Configuration
  uploadconfig_ptr_t upload_config;           // Upload configuration
  uploader_ptr_t uploader;                    // The uploader to use
  
  // Callbacks - using pysafe types for Python GIL compatibility
  pysafe_upload_progress_callback_t on_progress;
  pysafe_completion_callback_t on_complete;
  pysafe_error_callback_t on_error;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Constructor
  ////////////////////////////////////////////////////////////////////////////////
  ChunkUploadCoordinator(
      const std::string& id, 
      const std::string& hash,
      chunkmanifest_ptr_t manifest,
      const file::Path& chunks_dir,
      uploadconfig_ptr_t config,
      uploader_ptr_t upload_impl)
      : asset_id(id), 
        storage_hash(hash), 
        chunk_manifest(manifest), 
        total_chunks(manifest ? manifest->_chunks.size() : 0),
        chunks_directory(chunks_dir),
        upload_config(config),
        uploader(upload_impl) {
    // Initialize uploads vector
    active_uploads.atomicOp([this](upload_list_t& uploads) {
      uploads.reserve(total_chunks);
    });
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress calculation
  ////////////////////////////////////////////////////////////////////////////////
  float getOverallProgress() const {
    if (total_chunks == 0) return 0.0f;
    return static_cast<float>(chunks_uploaded.load()) / total_chunks;
  }
  
  bool isComplete() const {
    return chunks_uploaded.load() + chunks_failed.load() >= total_chunks;
  }
  
  bool hasSucceeded() const {
    return chunks_uploaded.load() == total_chunks;
  }
  
  bool hasFailed() const {
    return chunks_failed.load() > 0;
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Check if upload is complete and trigger callbacks
  ////////////////////////////////////////////////////////////////////////////////
  void checkCompletion() {
    if (isComplete() && !completed.exchange(true)) {
      // First time reaching completion
      if (hasSucceeded()) {
        if (on_complete._item) {
          // For chunk uploads, we don't have assembled data, just signal success
          on_complete._item(nullptr); 
        }
      } else {
        if (on_error._item) {
          std::string error_msg = FormatString(
            "Upload failed: %d/%d chunks failed", 
            chunks_failed.load(), 
            total_chunks
          );
          on_error._item(error_msg);
        }
      }
    }
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Report aggregate progress
  ////////////////////////////////////////////////////////////////////////////////
  void reportProgress() {
    if (on_progress._item) {
      UploadProgress progress;
      progress.current_file = asset_id;
      progress.files_completed = chunks_uploaded.load();
      progress.total_files = total_chunks;
      // Note: Individual chunk bytes tracking would require more state
      on_progress._item(progress);
    }
  }
};

} //namespace ork::asset::catalog {
