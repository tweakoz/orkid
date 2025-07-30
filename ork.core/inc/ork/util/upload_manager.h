////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/util/upload.h>
#include <ork/kernel/opq.h>
#include <memory>
#include <thread>
#include <atomic>

namespace ork {

struct UploadManager;
struct UploadGroup;
using uploadmanager_ptr_t = std::shared_ptr<UploadManager>;
using upload_group_ptr_t = std::shared_ptr<UploadGroup>;

struct UploadManager {
  //////////////////////////////////////////////////////////////////////////////
  // Public members
  //////////////////////////////////////////////////////////////////////////////
  opq::opq_ptr_t _work_queue;
  size_t _max_concurrent_uploads = 4;
  
  //////////////////////////////////////////////////////////////////////////////
  // Constructor/Destructor
  //////////////////////////////////////////////////////////////////////////////
  explicit UploadManager(opq::opq_ptr_t queue = nullptr);
  ~UploadManager();
  
  //////////////////////////////////////////////////////////////////////////////
  // Main interface - combined create and queue
  //////////////////////////////////////////////////////////////////////////////
  upload_ptr_t upload(const file::Path& source_path, const URL& dest_url);
  
  //////////////////////////////////////////////////////////////////////////////
  // Upload group support
  //////////////////////////////////////////////////////////////////////////////
  void uploadGroup(upload_group_ptr_t group);
  
  //////////////////////////////////////////////////////////////////////////////
  // Control methods
  //////////////////////////////////////////////////////////////////////////////
  void setMaxConcurrentUploads(size_t max);
  void shutdown();
  bool isActive() const;
  size_t activeUploadCount() const;
  
  //////////////////////////////////////////////////////////////////////////////
  // Global retry configuration
  // These defaults are used if not overridden per-upload
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
  
  // Process upload with retry logic
  // - Attempts upload
  // - On failure, checks shouldRetry()
  // - Schedules retry with exponential backoff
  // - Calls failure callback only after all retries exhausted
  void processUpload(upload_ptr_t ul);
  
  // Schedule a retry for failed upload
  // Uses OPQ to schedule with delay
  void scheduleRetry(upload_ptr_t ul);
  
  void updateActiveUploads();
};

} // namespace ork