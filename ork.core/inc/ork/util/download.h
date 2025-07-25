////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/kernel/svariant.h>
#include <ork/util/URL.h>
#include <ork/file/path.h>
#include <functional>
#include <memory>
#include <atomic>
#include <optional>
#include <map>

namespace ork {

struct Download;
using download_ptr_t = std::shared_ptr<Download>;

enum class DownloadState {
  PENDING,
  DOWNLOADING,
  COMPLETED,
  FAILED,
  CANCELLED
};

// Progress callback: (downloaded_bytes, total_bytes) -> void
using download_progress_fn_t = std::function<void(size_t, size_t)>;

// Completion callback: (success, file_path) -> void
using download_complete_fn_t = std::function<void(bool, const file::Path&)>;

// Failure callback: (error_message) -> void
using download_failure_fn_t = std::function<void(const std::string&)>;

struct Download {
  //////////////////////////////////////////////////////////////////////////////
  // Public members
  //////////////////////////////////////////////////////////////////////////////
  URL _url;
  file::Path _destination_path;
  std::optional<std::string> _api_key;
  bool _ignore_tls_errors = false;
  std::map<std::string, std::string> _headers;
  
  //////////////////////////////////////////////////////////////////////////////
  // State
  //////////////////////////////////////////////////////////////////////////////
  std::atomic<DownloadState> _state{DownloadState::PENDING};
  std::atomic<size_t> _downloaded_bytes{0};
  std::atomic<size_t> _total_bytes{0};
  std::string _error_message;
  
  //////////////////////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////////////////////
  ItemAndData<download_progress_fn_t> _on_progress;
  ItemAndData<download_complete_fn_t> _on_complete;
  ItemAndData<download_failure_fn_t> _on_failure;
  
  
  // Constructor
  Download(const URL& url, const file::Path& dest_path);
  
  // Methods
  void setHeader(const std::string& key, const std::string& value);
  void setApiKey(const std::string& key);
  
  //////////////////////////////////////////////////////////////////////////////
  // Internal use by DownloadManager
  //////////////////////////////////////////////////////////////////////////////
  void* _curl_handle = nullptr;  // CURL* handle
  
  // Helper to get progress percentage
  float progressPercentage() const;
};

} // namespace ork