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
#include <ork/asset/catalog/types.h>
#include <ork/util/logger.h>
#include <functional>
#include <memory>
#include <atomic>
#include <optional>
#include <map>
#include <vector>

namespace ork {

struct Upload;
using upload_ptr_t = std::shared_ptr<Upload>;

// Forward declarations
struct UploadConfig;
struct HttpsUploaderConfig;

enum class UploadState {
  PENDING,
  UPLOADING,
  COMPLETED,
  FAILED,
  CANCELLED
};

// Progress callback: (uploaded_bytes, total_bytes) -> void
using upload_progress_fn_t = std::function<void(size_t, size_t)>;

// Completion callback: (success, remote_url) -> void
using upload_complete_fn_t = std::function<void(bool, const URL&)>;

// Failure callback: (error_message) -> void
using upload_failure_fn_t = std::function<void(const std::string&)>;

// Python-safe wrappers
using pysafe_upload_progress_fn_t = ItemAndData<upload_progress_fn_t>;
using pysafe_upload_complete_fn_t = ItemAndData<upload_complete_fn_t>;
using pysafe_upload_failure_fn_t = ItemAndData<upload_failure_fn_t>;

struct Upload {
  //////////////////////////////////////////////////////////////////////////////
  // Public members
  //////////////////////////////////////////////////////////////////////////////
  file::Path _source_path;
  URL _destination_url;
  std::optional<std::string> _api_key;
  bool _ignore_tls_errors = false;
  
  //////////////////////////////////////////////////////////////////////////////
  // Additional headers for HTTP uploads
  //////////////////////////////////////////////////////////////////////////////
  std::map<std::string, std::string> _headers;
  
  //////////////////////////////////////////////////////////////////////////////
  // Authentication
  //////////////////////////////////////////////////////////////////////////////
  std::optional<std::string> _username;
  std::optional<std::string> _password;
  std::optional<file::Path> _ssh_key_path;  // For SCP uploads
  
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
  std::atomic<UploadState> _state{UploadState::PENDING};
  std::atomic<size_t> _bytes_uploaded{0};
  size_t _total_bytes = 0;
  std::string _error_message;
  
  //////////////////////////////////////////////////////////////////////////////
  // Retry state (managed by UploadManager)
  //////////////////////////////////////////////////////////////////////////////
  std::atomic<int> _retry_count{0};        // Current retry attempt
  std::atomic<int> _next_retry_delay_ms{0}; // Next retry delay (for backoff)
  
  //////////////////////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////////////////////
  ItemAndData<upload_progress_fn_t> _on_progress;
  ItemAndData<upload_complete_fn_t> _on_complete;
  ItemAndData<upload_failure_fn_t> _on_failure;
  
  //////////////////////////////////////////////////////////////////////////////
  // Methods
  //////////////////////////////////////////////////////////////////////////////
  Upload();
  ~Upload();
  
  // Execute the upload (blocking)
  bool execute();
  
  // Cancel the upload
  void cancel();
  
  // Get progress percentage (0.0 - 1.0)
  float getProgress() const;
  
  
  // Check if should retry based on error and retry count
  // Returns true if upload should be retried
  // Implementation considers:
  // - Current retry count vs max_retries
  // - Error type (network/auth errors retry differently)
  bool shouldRetry() const;
  
  // Calculate next retry delay with exponential backoff
  int getNextRetryDelay() const;
  
private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
  
  // Protocol-specific upload methods
  bool executeHTTP();
};

////////////////////////////////////////////////////////////////////////////////
// Base configuration for all upload protocols
////////////////////////////////////////////////////////////////////////////////

struct UploadConfig {
  // Common fields for all protocols
  std::string remote_base_path;
  bool create_directories = true;
  bool overwrite_existing = true;
  bool verify_uploads = true;
  bool compress_transfer = false;
  int timeout_seconds = 300;
  int retry_count = 3;
  
  // Upload manager for handling retries and queuing
  std::shared_ptr<struct UploadManager> upload_manager;
  
  // Virtual validation methods
  virtual bool isValid() const { return true; }
  virtual std::string getValidationError() const { return ""; }
  
  virtual ~UploadConfig() = default;
};

////////////////////////////////////////////////////////////////////////////////
// Abstract base class for protocol-specific uploaders
////////////////////////////////////////////////////////////////////////////////

class Uploader {
public:
  virtual ~Uploader() = default;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Main upload methods
  ////////////////////////////////////////////////////////////////////////////////
  
  // Upload multiple files
  virtual bool uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths
  ) = 0;
  
  // Upload a single file
  virtual bool uploadFile(
    const file::Path& local_file,
    const std::string& remote_path
  ) = 0;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Connection management
  ////////////////////////////////////////////////////////////////////////////////
  
  // Test connection to remote
  virtual bool testConnection() = 0;
  
  // Get uploader type name
  virtual std::string type() const = 0;
  
  // Check if file exists on remote
  virtual bool remoteFileExists(const std::string& remote_path) = 0;
  
  // Delete remote file
  virtual bool deleteRemoteFile(const std::string& remote_path) = 0;
  
  // List remote directory
  virtual std::vector<std::string> listRemoteDirectory(const std::string& path) = 0;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress tracking
  ////////////////////////////////////////////////////////////////////////////////
  
  void setProgressCallback(upload_progress_fn_t callback) { _progress_callback = callback; }

  // Per-file completion callback: (remote_path) -> void
  using file_completed_fn_t = std::function<void(const std::string&)>;
  void setFileCompletedCallback(file_completed_fn_t callback) { _file_completed_callback = callback; }

  // Cancel upload
  virtual void cancel() { _cancelled = true; }
  bool isCancelled() const { return _cancelled; }

  // External cancel flag (e.g. from UploadRequest) — checked by CURL progress callback
  void setExternalCancelFlag(std::atomic<bool>* flag) { _external_cancel = flag; }
  std::atomic<bool>* externalCancelFlag() const { return _external_cancel; }

protected:
  // Update progress
  void updateProgress(size_t uploaded, size_t total) {
    if (_progress_callback) {
      _progress_callback(uploaded, total);
    }
  }

protected:
  upload_progress_fn_t _progress_callback;
  file_completed_fn_t _file_completed_callback;
  std::atomic<bool> _cancelled{false};
  std::atomic<bool>* _external_cancel = nullptr;
};

using uploader_ptr_t = std::shared_ptr<Uploader>;


////////////////////////////////////////////////////////////////////////////////
// HTTPS uploader configuration
////////////////////////////////////////////////////////////////////////////////

struct HttpsUploaderConfig : public UploadConfig {
  std::string host;
  std::string api_key;
  std::string username;
  std::string password;
  int port = 443;
  bool verify_ssl = true;
  std::map<std::string, std::string> custom_headers;
  bool useTLS() const {
    // TODO FIXME
    return ((port%1000) == 443); // Common HTTPS ports
  }
  std::string protocol() const {
    return useTLS() ? "https" : "http"; // Common HTTPS ports
  }
  // Validation
  bool isValid() const override {
    return !host.empty() && (port > 0 && port < 65536);
  }
  
  std::string getValidationError() const override {
    if (host.empty()) return "Host is required";
    if (port <= 0 || port >= 65536) return "Invalid port number";
    return "";
  }
};

////////////////////////////////////////////////////////////////////////////////
// HTTPS uploader implementation (REST API)
////////////////////////////////////////////////////////////////////////////////

class HttpsUploader : public Uploader {
public:
  HttpsUploader(httpsuploaderconfig_ptr_t config);
  ~HttpsUploader() override;
  
  // Uploader interface
  bool uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths
  ) override;
  
  bool uploadFile(
    const file::Path& local_file,
    const std::string& remote_path
  ) override;
  
  bool testConnection() override;
  std::string type() const override { return "https"; }
  
  bool remoteFileExists(const std::string& remote_path) override;
  bool deleteRemoteFile(const std::string& remote_path) override;
  std::vector<std::string> listRemoteDirectory(const std::string& path) override;
  
  // Additional HTTP-specific methods
  void setCustomHeaders(const std::map<std::string, std::string>& headers);
  void setEndpointUrl(const URL& url);
  
  // Internal progress handling (called from CURL callback)
  void handleUploadProgress(size_t uploaded, size_t total);
  
  // Calculate MD5 hash of a local file
  static std::string calculateFileMD5(const file::Path& file_path);
  
  // Check if remote file has same content as local file (by comparing MD5)
  // Returns true if file exists on remote and MD5 matches
  bool remoteFileMatchesLocal(const file::Path& local_file, const std::string& remote_path);
  
private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
  httpsuploaderconfig_ptr_t _config;
  logchannel_ptr_t _log_channel; 
};



} // namespace ork