////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/util/download.h>
#include <ork/kernel/opq.h>
#include <memory>
#include <thread>
#include <atomic>

namespace ork {

struct DownloadManager;
struct DownloadGroup;
using downloadmanager_ptr_t = std::shared_ptr<DownloadManager>;
using download_group_ptr_t = std::shared_ptr<DownloadGroup>;

struct DownloadManager {
  //////////////////////////////////////////////////////////////////////////////
  // Public members
  //////////////////////////////////////////////////////////////////////////////
  opq::opq_ptr_t _work_queue;
  size_t _max_concurrent_downloads = 4;
  
  //////////////////////////////////////////////////////////////////////////////
  // Constructor/Destructor
  //////////////////////////////////////////////////////////////////////////////
  explicit DownloadManager(opq::opq_ptr_t queue = nullptr);
  ~DownloadManager();
  
  //////////////////////////////////////////////////////////////////////////////
  // Main interface - combined create and queue
  //////////////////////////////////////////////////////////////////////////////
  download_ptr_t download(const URL& url, const file::Path& dest_path);
  void enqueue(download_ptr_t dl);
  
  //////////////////////////////////////////////////////////////////////////////
  // Download group support
  //////////////////////////////////////////////////////////////////////////////
  void downloadGroup(download_group_ptr_t group);
  
  //////////////////////////////////////////////////////////////////////////////
  // Control methods
  //////////////////////////////////////////////////////////////////////////////
  void shutdown();
  bool isActive() const;
  size_t activeDownloadCount() const;
  
  //////////////////////////////////////////////////////////////////////////////
  // Utility methods
  //////////////////////////////////////////////////////////////////////////////
  // Check if a remote file exists using HEAD request
  // Returns true if file exists (HTTP 200), false otherwise
  bool remoteFileExists(const URL& url, 
                       const std::map<std::string, std::string>& headers = {},
                       bool ignore_tls_errors = false);
  
  //////////////////////////////////////////////////////////////////////////////
  // Global retry configuration
  // These defaults are used if not overridden per-download
  //////////////////////////////////////////////////////////////////////////////
  void setDefaultMaxRetries(int retries) { _default_max_retries = retries; }
  void setDefaultRetryDelay(int delay_ms) { _default_retry_delay_ms = delay_ms; }
  void setDefaultRetryBackoff(float multiplier) { _default_retry_backoff = multiplier; }
  
  struct Impl;
  
private:
  std::unique_ptr<Impl> _impl;
  
  // Default retry configuration
  int _default_max_retries = 3;
  int _default_retry_delay_ms = 1000;
  float _default_retry_backoff = 2.0f;
  
  // Process download with retry logic
  // - Attempts download
  // - On failure, checks shouldRetry()
  // - Schedules retry with exponential backoff
  // - Calls failure callback only after all retries exhausted
  void processDownload(download_ptr_t dl);
  
  // Schedule a retry for failed download
  // Uses OPQ to schedule with delay
  void scheduleRetry(download_ptr_t dl);
  
  void updateActiveDownloads();
};

} // namespace ork