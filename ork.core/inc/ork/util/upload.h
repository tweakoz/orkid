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
struct ScpUploaderConfig;
struct HttpsUploaderConfig;
struct S3UploaderConfig;

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
  
  // Get upload rate in bytes/sec
  double getUploadRate() const;
  
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
  bool executeSCP();
  bool executeS3();
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
  
  // Cancel upload
  virtual void cancel() { _cancelled = true; }
  bool isCancelled() const { return _cancelled; }
  
protected:
  // Update progress
  void updateProgress(size_t uploaded, size_t total) {
    if (_progress_callback) {
      _progress_callback(uploaded, total);
    }
  }
  
protected:
  upload_progress_fn_t _progress_callback;
  std::atomic<bool> _cancelled{false};
};

using uploader_ptr_t = std::shared_ptr<Uploader>;

////////////////////////////////////////////////////////////////////////////////
// SCP uploader configuration
////////////////////////////////////////////////////////////////////////////////

struct ScpUploaderConfig : public UploadConfig {
  std::string host;
  std::string username;
  std::string password;
  std::string key_file;              // SSH key file path
  int port = 22;
  
  // SSH ControlMaster support for 2FA/interactive auth
  bool use_control_master = false;   // Enable ControlMaster mode
  std::string control_path;          // Path to control socket (auto-generated if empty)
  bool auto_add_host_key = false;    // Automatically accept new host keys
  std::string ssh_options;           // Additional SSH options
  
  // Validation
  bool isValid() const override {
    return !host.empty() && !username.empty() && (port > 0 && port < 65536);
  }
  
  std::string getValidationError() const override {
    if (host.empty()) return "Host is required";
    if (username.empty()) return "Username is required";
    if (port <= 0 || port >= 65536) return "Invalid port number";
    return "";
  }
};

////////////////////////////////////////////////////////////////////////////////
// SCP uploader implementation
////////////////////////////////////////////////////////////////////////////////

class ScpUploader : public Uploader {
public:
  ScpUploader(scpuploaderconfig_ptr_t config);
  ~ScpUploader() override;
  
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
  std::string type() const override { return "scp"; }
  
  bool remoteFileExists(const std::string& remote_path) override;
  bool deleteRemoteFile(const std::string& remote_path) override;
  std::vector<std::string> listRemoteDirectory(const std::string& path) override;
  
  ////////////////////////////////////////////////////////////////////////////////
  // SSH ControlMaster management
  ////////////////////////////////////////////////////////////////////////////////
  
  // Check if a ControlMaster connection exists
  // Implementation should check if control socket file exists and is valid
  bool hasControlMaster() const;
  
  // Get the control socket path (generates one if not specified)
  // Default pattern: ~/.ssh/cm-%h-%p-%r (host, port, remote_user)
  std::string getControlPath() const;
  
  // Instructions for setting up ControlMaster
  // Returns multi-line string with setup commands for user
  static std::string getControlMasterSetupInstructions(
    const std::string& host,
    const std::string& username,
    int port = 22
  );
  
private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
  scpuploaderconfig_ptr_t _config;
  
  // Build SSH/SCP command with appropriate options
  // Should include -o ControlPath=... when use_control_master is true
  // Example: scp -o ControlPath=~/.ssh/cm-%h-%p-%r file.txt user@host:path/
  std::vector<std::string> buildScpCommand(
    const file::Path& local_file,
    const std::string& remote_path,
    bool upload = true
  ) const;
  
  // Build SSH command for remote operations (mkdir, ls, rm)
  // Should include same ControlPath options as SCP
  std::vector<std::string> buildSshCommand(
    const std::string& remote_command
  ) const;
  
  // Common SSH options based on config
  // Should return options like:
  // - "-o ControlPath=<path>" if use_control_master
  // - "-o ControlMaster=no" to prevent creating new masters
  // - "-o StrictHostKeyChecking=no" if auto_add_host_key
  // - Any additional options from ssh_options field
  std::vector<std::string> getSshOptions() const;
};

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

////////////////////////////////////////////////////////////////////////////////
// S3 uploader configuration
////////////////////////////////////////////////////////////////////////////////

struct S3UploaderConfig : public UploadConfig {
  std::string bucket;
  std::string region;
  std::string access_key_id;
  std::string secret_access_key;
  std::string storage_class = "STANDARD";
  std::string acl = "private";
  std::string endpoint_url;          // For S3-compatible services
  
  // Validation
  bool isValid() const override {
    return !bucket.empty() && !region.empty() && !access_key_id.empty() && !secret_access_key.empty();
  }
  
  std::string getValidationError() const override {
    if (bucket.empty()) return "Bucket is required";
    if (region.empty()) return "Region is required";
    if (access_key_id.empty()) return "Access key ID is required";
    if (secret_access_key.empty()) return "Secret access key is required";
    return "";
  }
};

////////////////////////////////////////////////////////////////////////////////
// S3 uploader implementation
////////////////////////////////////////////////////////////////////////////////

class S3Uploader : public Uploader {
public:
  S3Uploader(s3uploaderconfig_ptr_t config);
  ~S3Uploader() override;
  
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
  std::string type() const override { return "s3"; }
  
  bool remoteFileExists(const std::string& remote_path) override;
  bool deleteRemoteFile(const std::string& remote_path) override;
  std::vector<std::string> listRemoteDirectory(const std::string& path) override;
  
  // S3-specific configuration
  void setBucket(const std::string& bucket);
  void setRegion(const std::string& region);
  void setStorageClass(const std::string& storage_class);
  void setACL(const std::string& acl);
  
private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
  s3uploaderconfig_ptr_t _config;
};

////////////////////////////////////////////////////////////////////////////////
// Helper function to create an SCP uploader with ControlMaster support
//
// Usage for 2FA scenarios:
// 1. First, establish a ControlMaster connection manually:
//    ssh -M -S ~/.ssh/controlmaster-%h-%p-%r user@host
//    (This will prompt for password and 2FA code)
//
// 2. Keep that terminal open, then use this uploader:
//    auto config = std::make_shared<ScpUploader::Config>();
//    config->host = "example.com";
//    config->username = "user";
//    config->use_control_master = true;
//    auto uploader = createScpUploaderWithControlMaster(config);
//
// 3. The uploader will reuse the authenticated connection
//
// Alternative: Let the uploader auto-generate the control path:
//    config->control_path = ""; // Will use ~/.ssh/cm-%h-%p-%r
//
// IMPLEMENTATION NOTES:
// - The implementation should call hasControlMaster() before operations
// - If no ControlMaster exists, show getControlMasterSetupInstructions()
// - All SCP/SSH commands must include: -o ControlMaster=no
// - This prevents the uploader from creating new master connections
// - Consider adding a testConnection() call that checks ControlMaster validity
////////////////////////////////////////////////////////////////////////////////

uploader_ptr_t createScpUploaderWithControlMaster(
  const std::string& host,
  const std::string& username,
  const std::string& remote_base_path,
  int port = 22
);

} // namespace ork