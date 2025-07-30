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

// Python-safe wrappers
using pysafe_download_progress_fn_t = ItemAndData<download_progress_fn_t>;
using pysafe_download_complete_fn_t = ItemAndData<download_complete_fn_t>;
using pysafe_download_failure_fn_t = ItemAndData<download_failure_fn_t>;

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
  // Retry configuration
  //////////////////////////////////////////////////////////////////////////////
  int _max_retries = 3;                     // Maximum retry attempts
  int _retry_delay_ms = 1000;               // Initial delay between retries
  float _retry_backoff_multiplier = 2.0f;   // Exponential backoff multiplier
  int _max_retry_delay_ms = 30000;          // Cap on retry delay (30 seconds)
  
  //////////////////////////////////////////////////////////////////////////////
  // State
  //////////////////////////////////////////////////////////////////////////////
  std::atomic<DownloadState> _state{DownloadState::PENDING};
  std::atomic<size_t> _downloaded_bytes{0};
  std::atomic<size_t> _total_bytes{0};
  std::string _error_message;
  
  //////////////////////////////////////////////////////////////////////////////
  // Retry state (managed by DownloadManager)
  //////////////////////////////////////////////////////////////////////////////
  std::atomic<int> _retry_count{0};        // Current retry attempt
  std::atomic<int> _next_retry_delay_ms{0}; // Next retry delay (for backoff)
  
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
  void* _manager_impl = nullptr; // DownloadManager::Impl* for performance tracking
  
  // Helper to get progress percentage
  float progressPercentage() const;
  
  // Check if should retry based on error and retry count
  // Returns true if download should be retried
  // Implementation considers:
  // - Current retry count vs max_retries
  // - Error type (network errors retry, others don't)
  bool shouldRetry() const;
  
  // Calculate next retry delay with exponential backoff
  int getNextRetryDelay() const;
};

} // namespace ork