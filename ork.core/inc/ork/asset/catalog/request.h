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
  assetid_t _original_fqid;        // Original fully-qualified ID (namespace|asset)
  namespaceid_t _namespace_id;     // Namespace the asset belongs to
  assetnamespace_ptr_t _namespace; // Resolved namespace (if any)
  assetid_t _asset_id;             // asset ID (within the namespace)
  locationinfo_ptr_t _location_info;   // Configuration for this location
  assetentry_ptr_t _asset_info;    // resolved asset info (if any)
  file::Path _pak_local_path;      // local path where pak was created (if any)
  file::Path  _source_dir;         // local source directory used to create pak (if any)
  std::string _resolved_base_url;  // base URL for downloads (if any)
};

//TODO: hoist all fqid parsing to one place AssetFqIdentifier::parse(const std::string& fqid);

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
// Result of an asset retrieval operation
////////////////////////////////////////////////////////////////////////////////

struct FetchRequest {

  assetfqid_ptr_t _fqid;
  bool _enable_caching = true;
  datablock_ptr_t _data;                  // The actual asset data (if successful)
                                          // For asset_pak: nullptr (use _pak_contents instead)
  AssetStatus _status = AssetStatus::OK;  // Using CrcEnum for Python compatibility
  std::string _error_detail;              // Additional context for debugging
  
  static void invokeCompletionCallbacks(fetchrequest_ptr_t request);
  // For asset_pak types: map of extracted files
  // Key: relative path within the tar (e.g., "models/character.obj")
  // Value: datablock containing the file contents
  std::map<std::string, datablock_ptr_t> _pak_contents;
  LockedResource<asset_callback_list_t> _completion_callbacks; // Callbacks to invoke on completion
  // Performance metrics
  double _download_time = 0.0;         // Time spent downloading
  double _processing_time = 0.0;       // Time spent decrypting/decompressing
  
  std::atomic<AssetState> _state{AssetState::NEW};
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

  bool wait();
  bool isComplete() const;  

  // Helpers
  bool isSuccess() const;
  operator bool() const;  // Allow if(result) syntax
  bool isPak() const { return !_pak_contents.empty(); }  // Check if this is a pak result
};


} //  namespace ork::asset::catalog {