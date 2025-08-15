namespace ork::asset::catalog {

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
  
  // Asset index entry
  struct AssetIndexEntry {
    namespaceid_t namespace_id;
    std::string asset_path;
    assetmanifest_ptr_t manifest;
    assetentry_ptr_t entry;
  };
  
  // Download task
  struct DownloadTask {
    task_fn_t task;
    std::string asset_id;
    int priority;
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

  CatalogImpl(AssetCatalog* catalog) : _catalog(catalog), _generation(1) {
    // State is now initialized via LockedResource default constructor
    
    // Initialize root namespace
    _root_namespace = std::make_shared<AssetNamespace>("");
    _root_namespace->_full_path = "";
        
    // Initialize download manager with default concurrent queue
    _download_manager = std::make_shared<DownloadManager>(opq::concurrentQueue());
  }
  
  AssetCatalog* _catalog;
  
  // Single source of truth for catalog version (still used for cache invalidation)
  std::atomic<uint64_t> _generation;
  
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
  LockedResource<pysafe_download_progress_callback_t> _global_progress_callback;  // Python-bindable
  
  // Statistics - thread-safe via LockedResource
  mutable LockedResource<Stats> _stats;
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // Internal Methods moved from header
  ////////////////////////////////////////////////////////////////////////////////
  
  // Build global asset index from loaded manifests
  void rebuildAssetIndex();
  
  // Locate asset in manifests
  assetlocation_ptr_t locateAsset(const std::string& fq_asset_id) const;
  
  // Atomic file download - just gets bytes from a URL
  datablock_ptr_t downloadFile(const std::string& url, const locationinfo_ptr_t& location_info = nullptr);
  
  // High-level asset retrieval (new refactored method)
  assetresult_ptr_t getAsset(
    const assetid_t& fq_asset_id,
    const AssetLocation& location,
    const assetentry_ptr_t& asset_info,
    bool decrypt
  );
  
  // Download phases
  datablock_ptr_t downloadAssetData(const AssetLocation& location);
  datablock_ptr_t downloadChunkedData(const AssetLocation& location);
  datablock_ptr_t downloadSingleData(const AssetLocation& location);
  
  // Processing phases
  datablock_ptr_t processAssetData(
    datablock_ptr_t data,
    const AssetLocation& location,
    bool decrypt
  );
  datablock_ptr_t decryptData(
    datablock_ptr_t data,
    const namespaceid_t& namespace_id
  );
  datablock_ptr_t decompressData(
    datablock_ptr_t data,
    CompressionType compression_type
  );
  
  // Asset type handlers
  void handleAssetPak(datablock_ptr_t data, AssetResult& result);
  void handleRegularAsset(datablock_ptr_t data, AssetResult& result);
  void writeAssetPakToLocal(const assetentry_ptr_t& asset_info, AssetResult& result);
    
  // Process download task (called by DownloadManager)
  void processDownloadTask(const DownloadTask& task);
  
  // Update download progress
  void updateDownloadProgress(
    const std::string& asset_id,
    size_t current,
    size_t total
  );
  
  // Parse fully qualified asset ID into namespace and asset path
  std::pair<std::string, std::string> parseAssetId(const std::string& fq_asset_id) const;
  
  // Convert wildcard pattern to regex
  static std::regex wildcardToRegex(const std::string& pattern);

chunkdownloadcoordinator_ptr_t downloadChunkedAsset(
    const assetid_t& fq_asset_id,
    const AssetLocation& location,
    const pysafe_completion_callback_t& on_complete,
    const pysafe_error_callback_t& on_error);
chunkdownloadcoordinator_ptr_t downloadNonChunkedAsset(
    const assetid_t& fq_asset_id,
    const AssetLocation& location,
    const pysafe_completion_callback_t& on_complete,
    const pysafe_error_callback_t& on_error);

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
  // Called by download completion callback for each chunk
  ////////////////////////////////////////////////////////////////////////////////
  void onChunkDownloaded(int chunk_index, const file::Path& chunk_path) {
    // TODO: Implement
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Called when a chunk fails to download
  ////////////////////////////////////////////////////////////////////////////////
  void onChunkFailed(int chunk_index, const std::string& error) {
    // TODO: Implement
  }
  
  ////////////////////////////////////////////////////////////////////////////////
  // Assembly coordination
  ////////////////////////////////////////////////////////////////////////////////
  void triggerAssembly() {
    // TODO: Implement
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




} //namespace ork::asset::catalog {
