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

struct AssetFqIdentifier {
  namespaceid_t _namespace_id;     // Namespace the asset belongs to
  assetid_t _asset_id;             // asset ID (within the namespace)
  assetlocation_ptr_t _location;  // resolved location info (if any)
};
//TODO: hoist all fqid parsing to one place AssetFqIdentifier::parse(const std::string& fqid);

////////////////////////////////////////////////////////////////////////////////
// Result of an asset retrieval operation
////////////////////////////////////////////////////////////////////////////////

struct AssetResult {
  datablock_ptr_t _data;               // The actual asset data (if successful)
                                      // For asset_pak: nullptr (use _pak_contents instead)
  assetlocation_ptr_t _location;             // Where the asset came from
  AssetStatus _status = AssetStatus::OK;  // Using CrcEnum for Python compatibility
  std::string _error_detail;           // Additional context for debugging
  
  // For asset_pak types: map of extracted files
  // Key: relative path within the tar (e.g., "models/character.obj")
  // Value: datablock containing the file contents
  std::map<std::string, datablock_ptr_t> _pak_contents;
  
  // Performance metrics
  double _download_time = 0.0;         // Time spent downloading
  double _processing_time = 0.0;       // Time spent decrypting/decompressing
  size_t _bytes_downloaded = 0;        // Total bytes downloaded (may be less than data size if compressed)
  
  // Helpers
  bool isSuccess() const;
  operator bool() const;  // Allow if(result) syntax
  bool isPak() const { return !_pak_contents.empty(); }  // Check if this is a pak result
};

////////////////////////////////////////////////////////////////////////////////
// Progress information for active downloads
////////////////////////////////////////////////////////////////////////////////

struct DownloadProgress {
  assetid_t _asset_id;
  size_t _bytes_downloaded = 0;
  size_t _total_bytes = 0;
  double _start_time = 0;
  double _rate = 0;                    // bytes/sec
  int _chunks_completed = 0;
  int _total_chunks = 0;
  
  // Get human-readable rate string (e.g., "1.5 MB/s")
  std::string getRateString() const;
};

////////////////////////////////////////////////////////////////////////////////
// AssetFuture - Represents a pending async asset fetch operation
////////////////////////////////////////////////////////////////////////////////

struct AssetFuture {
  assetid_t _asset_id;
  std::atomic<bool> _is_complete{false};
  std::atomic<bool> _is_cancelled{false};
  assetresult_ptr_t _result;
  std::mutex _mutex;
  std::condition_variable _cv;
  
  // Internal state for tracking
  fetchrequest_ptr_t _fetch_request;
  
  // Wait for completion (blocking)
  assetresult_ptr_t wait();
  
  // Check if complete (non-blocking)
  bool isComplete() const { return _is_complete.load(); }
  
  // Cancel the operation
  void cancel();
  
  // Get result if ready (non-blocking, returns nullptr if not ready)
  assetresult_ptr_t getResult() const;
};

////////////////////////////////////////////////////////////////
// FetchRequest - Encapsulates all parameters for asset fetching
////////////////////////////////////////////////////////////////

struct FetchRequest {
  assetfqid_ptr_t _fqid;
  assetentry_ptr_t asset_info;
  bool decrypt = true;
  bool disable_cache = false;
  // Future expansion: priority, timeout, retry_count, etc.
};

////////////////////////////////////////////////////////////////////////////////

struct AssetHandle {
  
  //////////////////////////////////////////////////////////////////////////////
  // Configuration
  //////////////////////////////////////////////////////////////////////////////
  
  namespaceid_t _namespace;          // Catalog namespace to use
  assetid_t _asset_id;           // Specific asset ID (optional)
  
  //////////////////////////////////////////////////////////////////////////////
  // State tracking (for flyweight pattern)
  //////////////////////////////////////////////////////////////////////////////
  
  std::atomic<AssetState> _state{AssetState::NOT_AVAILABLE};
  std::atomic<float> _progress{0.0f};
  std::atomic<size_t> _bytes_downloaded{0};
  std::atomic<size_t> _bytes_total{0};
  LockedResource<std::string> _error_message;
  std::atomic<int> _retry_count{0};
  std::atomic<size_t> _chunks_completed{0};
  std::atomic<size_t> _chunks_total{0};
  std::atomic<int> _refcount{0};     // Number of active requests
  
  //////////////////////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////////////////////
  
  pysafe_asset_request_progress_t _progress_callback;
  
  //////////////////////////////////////////////////////////////////////////////
  // Methods
  //////////////////////////////////////////////////////////////////////////////
  
  AssetHandle();
  AssetHandle(const namespaceid_t& ns);
  AssetHandle(const namespaceid_t& ns, const assetid_t& asset_id);
  
  bool isValid() const;
};


} //  namespace ork::asset::catalog {