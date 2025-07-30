////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/upload.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <fstream>
#include <atomic>
#include <mutex>

namespace ork {

//////////////////////////////////////////////////////////////////////////////
// CURL Callbacks for uploads
//////////////////////////////////////////////////////////////////////////////

// CURL callback for reading data to upload
static size_t read_callback(void* buffer, size_t size, size_t nmemb, void* userp) {
  auto* stream = static_cast<std::ifstream*>(userp);
  size_t buffer_size = size * nmemb;
  
  if (stream->eof()) {
    return 0;
  }
  
  stream->read(static_cast<char*>(buffer), buffer_size);
  size_t bytes_read = stream->gcount();
  
  return bytes_read;
}

// CURL callback for writing response data
static size_t write_response_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  // For uploads, we typically don't need the response body
  // but we need to consume it to avoid CURL errors
  if (userp) {
    // If userp is provided, it's a string* for capturing response
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), size * nmemb);
  }
  return size * nmemb;
}

// CURL callback for upload progress
static int upload_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                    curl_off_t ultotal, curl_off_t ulnow) {
  auto* uploader = static_cast<HttpsUploader*>(clientp);
  
  if (uploader->isCancelled()) {
    return 1;  // Abort transfer
  }
  
  // The HttpsUploader will handle progress internally through a public method
  if (ultotal > 0) {
    uploader->handleUploadProgress(static_cast<size_t>(ulnow), static_cast<size_t>(ultotal));
  }
  
  return 0;  // Continue
}

struct Upload::Impl {
  // TODO: Add implementation details
};

struct HttpsUploader::Impl {
  CURL* _curl_handle = nullptr;
  struct curl_slist* _headers = nullptr;
  
  // Progress tracking
  size_t _last_uploaded_bytes = 0;
  Timer _upload_timer;
  double _last_upload_rate = 0.0;
  
  ~Impl() {
    if (_curl_handle) {
      curl_easy_cleanup(_curl_handle);
    }
    if (_headers) {
      curl_slist_free_all(_headers);
    }
  }
};

struct ScpUploader::Impl {
  // TODO: Add SSH/SCP state
};

struct S3Uploader::Impl {
  // TODO: Add S3 SDK state
};

Upload::Upload()
    : _impl(std::make_unique<Impl>()) {
}

Upload::~Upload() {
}

bool Upload::execute() {
  _state = UploadState::UPLOADING;
  
  // Check if file exists
  if (!_source_path.doesPathExist()) {
    _state = UploadState::FAILED;
    _error_message = "Source file does not exist";
    return false;
  }
  
  // TODO: Implement actual upload based on protocol
  bool success = false;
  
  if (_destination_url._scheme == "http" || _destination_url._scheme == "https") {
    success = executeHTTP();
  } else if (_destination_url._scheme == "scp") {
    success = executeSCP();
  } else if (_destination_url._scheme == "s3") {
    success = executeS3();
  } else {
    _state = UploadState::FAILED;
    _error_message = "Unsupported protocol: " + _destination_url._scheme;
    return false;
  }
  
  if (success) {
    _state = UploadState::COMPLETED;
  } else {
    _state = UploadState::FAILED;
  }
  
  return success;
}

void Upload::cancel() {
  _state = UploadState::CANCELLED;
}

float Upload::getProgress() const {
  if (_total_bytes == 0) {
    return 0.0f;
  }
  return static_cast<float>(_bytes_uploaded) / static_cast<float>(_total_bytes);
}

double Upload::getUploadRate() const {
  // TODO: Implement upload rate calculation
  return 0.0;
}

bool Upload::shouldRetry() const {
  if (_retry_count >= _max_retries) {
    return false;
  }
  
  // TODO: Implement error-specific retry logic
  // For now, always retry on failure unless cancelled
  return _state == UploadState::FAILED;
}

int Upload::getNextRetryDelay() const {
  // Calculate exponential backoff
  int delay = _retry_delay_ms;
  for (int i = 0; i < _retry_count; ++i) {
    delay = static_cast<int>(delay * _retry_backoff_multiplier);
  }
  
  // Cap at max delay
  if (delay > _max_retry_delay_ms) {
    delay = _max_retry_delay_ms;
  }
  
  return delay;
}

bool Upload::executeHTTP() {
  // Create HttpsUploader config from URL and upload settings
  auto config = std::make_shared<HttpsUploaderConfig>();
  config->host = _destination_url._host;
  config->port = _destination_url._port > 0 ? _destination_url._port : 
                (_destination_url._scheme == "https" ? 443 : 80);
  config->verify_ssl = (_destination_url._scheme == "https") && !_ignore_tls_errors;
  config->remote_base_path = "";
  
  // Set authentication
  if (_api_key.has_value()) {
    config->api_key = _api_key.value();
  }
  if (_username.has_value()) {
    config->username = _username.value();
  }
  if (_password.has_value()) {
    config->password = _password.value();
  }
  
  // Set custom headers
  config->custom_headers = _headers;
  
  // Create uploader
  HttpsUploader uploader(config);
  
  // Set progress callback to update our state
  uploader.setProgressCallback([this](size_t uploaded, size_t total) {
    _bytes_uploaded = uploaded;
    _total_bytes = total;
    if (_on_progress._item) {
      _on_progress._item(uploaded, total);
    }
  });
  
  // Check if cancelled
  if (_state == UploadState::CANCELLED) {
    return false;
  }
  
  // Perform the upload
  bool success = uploader.uploadFile(_source_path, _destination_url._path);
  
  if (success) {
    if (_on_complete._item) {
      _on_complete._item(true, _destination_url);
    }
  } else {
    // Get more specific error information
    auto logchan = logger()->getChannel("UPLOAD");
    if (logchan) {
      logchan->log("HttpsUploader::uploadFile failed for %s", _source_path.c_str());
    }
    _error_message = "Upload failed";
    if (_on_failure._item) {
      _on_failure._item(_error_message);
    }
  }
  
  return success;
}

bool Upload::executeSCP() {
  // TODO: Implement SCP upload
  _error_message = "SCP upload not implemented";
  return false;
}

bool Upload::executeS3() {
  // TODO: Implement S3 upload
  _error_message = "S3 upload not implemented";
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Uploader base class
////////////////////////////////////////////////////////////////////////////////

// Destructor is already defined as default in the header

////////////////////////////////////////////////////////////////////////////////
// HttpsUploader
////////////////////////////////////////////////////////////////////////////////

HttpsUploader::HttpsUploader(httpsuploaderconfig_ptr_t config)
    : _config(config) {
  _impl = std::make_unique<Impl>();
}

HttpsUploader::~HttpsUploader() = default;

bool HttpsUploader::uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths) {
  // TODO: Implement batch upload
  for (size_t i = 0; i < local_files.size(); ++i) {
    if (!uploadFile(local_files[i], remote_paths[i])) {
      return false;
    }
  }
  return true;
}

bool HttpsUploader::uploadFile(
    const file::Path& local_file,
    const std::string& remote_path) {
  
  
  if (_cancelled) {
    printf("[HTTPS_ERROR] Upload cancelled\n");
    return false;
  }
  
  // Check if file exists
  if (!local_file.doesPathExist()) {
    printf("[HTTPS_ERROR] Local file doesn't exist: '%s'\n", local_file.c_str());
    return false;
  }
  
  // Get file size
  File file(local_file, EFM_READ);
  size_t file_size = 0;
  file.GetLength(file_size);
  file.Close();
  
  // Open input file
  std::ifstream input_file(local_file.c_str(), std::ios::binary);
  if (!input_file.is_open()) {
    return false;
  }
  
  // Always create a fresh CURL handle for each upload to ensure thread safety
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }
  
  // Build full URL - always use HTTPS for port 443
  std::string protocol = (_config->port == 443 || _config->port == 8443) ? "https" : "http";
  
  // Ensure proper path construction
  std::string base_path = _config->remote_base_path;
  if (!base_path.empty() && !base_path.starts_with("/")) {
    base_path = "/" + base_path;
  }
  if (!base_path.empty() && !base_path.ends_with("/")) {
    base_path += "/";
  }
  if (base_path.empty()) {
    base_path = "/";
  }
  
  std::string final_remote_path = remote_path;
  if (final_remote_path.starts_with("/")) {
    final_remote_path = final_remote_path.substr(1);
  }
  
  std::string full_url = FormatString("%s://%s:%d%s%s",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port,
    base_path.c_str(),
    final_remote_path.c_str());
  
  
  // Reset for new request
  curl_easy_reset(curl);
  
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
  
  // Set to PUT method
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  
  // Set file size
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_size);
  
  // Set read callback
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &input_file);
  
  // Set write callback to consume response
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
  
  // Set progress callback
  _impl->_last_uploaded_bytes = 0;
  _impl->_upload_timer.Start();
  
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, upload_progress_callback);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  
  // Set timeout
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, _config->timeout_seconds);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Build headers - create fresh list for each request to avoid thread safety issues
  struct curl_slist* request_headers = nullptr;
  
  // Add API key if present
  if (!_config->api_key.empty()) {
    std::string api_header = "X-API-Key: " + _config->api_key;
    request_headers = curl_slist_append(request_headers, api_header.c_str());
  }
  
  // Add basic auth if present
  if (!_config->username.empty()) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, _config->username.c_str());
    if (!_config->password.empty()) {
      curl_easy_setopt(curl, CURLOPT_PASSWORD, _config->password.c_str());
    }
  }
  
  // Add custom headers
  for (const auto& [key, value] : _config->custom_headers) {
    std::string header = key + ": " + value;
    request_headers = curl_slist_append(request_headers, header.c_str());
  }
  
  if (request_headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
  }
  
  // Log upload start
  auto logchan = logger()->getChannel("UPLOAD");
  if (logchan) {
    logchan->log("Uploading %s to %s", local_file.c_str(), full_url.c_str());
  }
  
  // Perform the upload
  CURLcode res = curl_easy_perform(curl);
  
  // Close input file
  input_file.close();
  
  // Free headers
  if (request_headers) {
    curl_slist_free_all(request_headers);
  }
  
  // Check result
  bool success = false;
  if (res == CURLE_OK) {
    // Get HTTP response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code >= 200 && response_code < 300) {
      if (logchan) {
        logchan->log("Upload successful: HTTP %ld", response_code);
      }
      success = true;
    } else {
      if(response_code!=429){
        printf("[HTTPS_ERROR] Upload failed with HTTP %ld\n", response_code);
      }
      if (logchan) {
        logchan->log("Upload failed: HTTP %ld", response_code);
      }
    }
  } else {
    if (logchan) {
      logchan->log("Upload failed: %s", curl_easy_strerror(res));
    }
  }
  
  // Clean up the fresh CURL handle
  curl_easy_cleanup(curl);
  
  return success;
}

bool HttpsUploader::testConnection() {
  // TODO: Test HTTPS connection
  return true;
}

bool HttpsUploader::remoteFileExists(const std::string& remote_path) {
  // Use HEAD request to check if file exists
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }
  
  // Build full URL - same logic as uploadFile
  std::string protocol = (_config->port == 443 || _config->port == 8443) ? "https" : "http";
  
  // Ensure proper path construction
  std::string base_path = _config->remote_base_path;
  if (!base_path.empty() && !base_path.starts_with("/")) {
    base_path = "/" + base_path;
  }
  if (!base_path.empty() && !base_path.ends_with("/")) {
    base_path += "/";
  }
  if (base_path.empty()) {
    base_path = "/";
  }
  
  std::string final_remote_path = remote_path;
  if (final_remote_path.starts_with("/")) {
    final_remote_path = final_remote_path.substr(1);
  }
  
  std::string full_url = FormatString("%s://%s:%d%s%s",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port,
    base_path.c_str(),
    final_remote_path.c_str());
  
  // Reset for new request
  curl_easy_reset(curl);
  
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
  
  // Use HEAD method
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Build headers
  struct curl_slist* headers = nullptr;
  
  // Add API key if present
  if (!_config->api_key.empty()) {
    std::string api_header = "X-API-Key: " + _config->api_key;
    headers = curl_slist_append(headers, api_header.c_str());
  }
  
  // Add basic auth if present
  if (!_config->username.empty()) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, _config->username.c_str());
    if (!_config->password.empty()) {
      curl_easy_setopt(curl, CURLOPT_PASSWORD, _config->password.c_str());
    }
  }
  
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  
  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  
  // Free headers
  if (headers) {
    curl_slist_free_all(headers);
  }
  
  // Check result
  bool exists = false;
  if (res == CURLE_OK) {
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    exists = (response_code == 200);
  }
  
  // Clean up CURL handle
  curl_easy_cleanup(curl);
  
  return exists;
}

bool HttpsUploader::deleteRemoteFile(const std::string& remote_path) {
  // TODO: Delete via DELETE request
  return false;
}

std::vector<std::string> HttpsUploader::listRemoteDirectory(const std::string& path) {
  // Use /api/list endpoint to get all files
  std::vector<std::string> files;
  
  CURL* curl = curl_easy_init();
  if (!curl) {
    return files;
  }
  
  // Build API URL
  std::string protocol = (_config->port == 443 || _config->port == 8443) ? "https" : "http";
  std::string full_url = FormatString("%s://%s:%d/api/list",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port);
  
  // Reset for new request
  curl_easy_reset(curl);
  
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
  
  // Response buffer
  std::string response_buffer;
  
  // Set write callback to capture response
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
    +[](void* contents, size_t size, size_t nmemb, std::string* userp) -> size_t {
      size_t total_size = size * nmemb;
      userp->append((char*)contents, total_size);
      return total_size;
    });
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Build headers
  struct curl_slist* headers = nullptr;
  
  // Add API key if present
  if (!_config->api_key.empty()) {
    std::string api_header = "X-API-Key: " + _config->api_key;
    headers = curl_slist_append(headers, api_header.c_str());
  }
  
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  
  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  
  // Free headers
  if (headers) {
    curl_slist_free_all(headers);
  }
  
  // Parse response if successful
  if (res == CURLE_OK) {
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code == 200 && !response_buffer.empty()) {
      // Parse JSON response
      rapidjson::Document doc;
      doc.Parse(response_buffer.c_str());
      
      if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("files")) {
        const auto& files_array = doc["files"];
        if (files_array.IsArray()) {
          for (rapidjson::SizeType i = 0; i < files_array.Size(); i++) {
            if (files_array[i].IsObject() && files_array[i].HasMember("name")) {
              const auto& name = files_array[i]["name"];
              if (name.IsString()) {
                files.push_back(name.GetString());
              }
            }
          }
        }
      }
    }
  }
  
  // Clean up CURL handle
  curl_easy_cleanup(curl);
  
  return files;
}

void HttpsUploader::setCustomHeaders(const std::map<std::string, std::string>& headers) {
  _config->custom_headers = headers;
}

void HttpsUploader::setEndpointUrl(const URL& url) {
  _config->host = url._host;
  _config->port = url._port;
  _config->remote_base_path = url._path;
}

void HttpsUploader::handleUploadProgress(size_t uploaded, size_t total) {
  // Update rate tracking
  if (_impl->_last_uploaded_bytes < uploaded) {
    double elapsed = _impl->_upload_timer.SecsSinceStart();
    if (elapsed > 0) {
      size_t bytes_delta = uploaded - _impl->_last_uploaded_bytes;
      _impl->_last_upload_rate = bytes_delta / elapsed;
    }
    _impl->_last_uploaded_bytes = uploaded;
  }
  
  // Call the progress callback
  if (_progress_callback) {
    _progress_callback(uploaded, total);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ScpUploader
////////////////////////////////////////////////////////////////////////////////

ScpUploader::ScpUploader(scpuploaderconfig_ptr_t config)
    : _config(config) {
  _impl = std::make_unique<Impl>();
}

ScpUploader::~ScpUploader() = default;

bool ScpUploader::uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths) {
  // TODO: Implement batch upload
  for (size_t i = 0; i < local_files.size(); ++i) {
    if (!uploadFile(local_files[i], remote_paths[i])) {
      return false;
    }
  }
  return true;
}

bool ScpUploader::uploadFile(
    const file::Path& local_file,
    const std::string& remote_path) {
  // TODO: Implement SCP upload
  return false;
}

bool ScpUploader::testConnection() {
  // TODO: Test SSH connection
  return true;
}

bool ScpUploader::remoteFileExists(const std::string& remote_path) {
  // TODO: Check via SSH
  return false;
}

bool ScpUploader::deleteRemoteFile(const std::string& remote_path) {
  // TODO: Delete via SSH
  return false;
}

std::vector<std::string> ScpUploader::listRemoteDirectory(const std::string& path) {
  // TODO: List via SSH
  return {};
}

bool ScpUploader::hasControlMaster() const {
  if (_config->control_path.empty()) {
    return false;
  }
  // TODO: Check if control socket exists
  return false;
}

std::string ScpUploader::getControlPath() const {
  if (!_config->control_path.empty()) {
    return _config->control_path;
  }
  // Generate default control path
  return FormatString("~/.ssh/cm-%s-%d-%s", 
    _config->host.c_str(), 
    _config->port, 
    _config->username.c_str());
}

std::string ScpUploader::getControlMasterSetupInstructions(
    const std::string& host,
    const std::string& username,
    int port) {
  return FormatString(
    "To set up SSH ControlMaster for 2FA:\n"
    "1. Open a new terminal\n"
    "2. Run: ssh -M -S ~/.ssh/cm-%s-%d-%s %s@%s -p %d\n"
    "3. Enter your password and 2FA code when prompted\n"
    "4. Keep this terminal open while uploading\n",
    host.c_str(), port, username.c_str(),
    username.c_str(), host.c_str(), port
  );
}

std::vector<std::string> ScpUploader::buildScpCommand(
    const file::Path& local_file,
    const std::string& remote_path,
    bool upload) const {
  std::vector<std::string> cmd;
  cmd.push_back("scp");
  
  // Add common SSH options
  auto options = getSshOptions();
  cmd.insert(cmd.end(), options.begin(), options.end());
  
  // Add port
  cmd.push_back("-P");
  cmd.push_back(std::to_string(_config->port));
  
  // Add source and destination
  if (upload) {
    cmd.push_back(local_file.c_str());
    cmd.push_back(FormatString("%s@%s:%s", 
      _config->username.c_str(), 
      _config->host.c_str(), 
      remote_path.c_str()));
  } else {
    cmd.push_back(FormatString("%s@%s:%s", 
      _config->username.c_str(), 
      _config->host.c_str(), 
      remote_path.c_str()));
    cmd.push_back(local_file.c_str());
  }
  
  return cmd;
}

std::vector<std::string> ScpUploader::buildSshCommand(
    const std::string& remote_command) const {
  std::vector<std::string> cmd;
  cmd.push_back("ssh");
  
  // Add common SSH options
  auto options = getSshOptions();
  cmd.insert(cmd.end(), options.begin(), options.end());
  
  // Add port
  cmd.push_back("-p");
  cmd.push_back(std::to_string(_config->port));
  
  // Add target
  cmd.push_back(FormatString("%s@%s", 
    _config->username.c_str(), 
    _config->host.c_str()));
  
  // Add command
  cmd.push_back(remote_command);
  
  return cmd;
}

std::vector<std::string> ScpUploader::getSshOptions() const {
  std::vector<std::string> options;
  
  if (_config->use_control_master) {
    options.push_back("-o");
    options.push_back("ControlMaster=no");
    options.push_back("-o");
    options.push_back(FormatString("ControlPath=%s", getControlPath().c_str()));
  }
  
  if (_config->auto_add_host_key) {
    options.push_back("-o");
    options.push_back("StrictHostKeyChecking=no");
  }
  
  if (!_config->ssh_options.empty()) {
    // Parse additional options
    // TODO: Properly parse ssh_options string
  }
  
  return options;
}

////////////////////////////////////////////////////////////////////////////////
// HttpsUploader - MD5 and duplicate check methods
////////////////////////////////////////////////////////////////////////////////

std::string HttpsUploader::calculateFileMD5(const file::Path& file_path) {
  // Use system md5sum command for now (cross-platform later)
  std::string cmd = FormatString("md5sum '%s' | cut -d' ' -f1", file_path.c_str());
  
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return "";
  }
  
  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);
  
  // Remove trailing whitespace
  result.erase(result.find_last_not_of(" \n\r\t") + 1);
  return result;
}

bool HttpsUploader::remoteFileMatchesLocal(const file::Path& local_file, const std::string& remote_path) {
  // Calculate local file MD5
  std::string local_md5 = calculateFileMD5(local_file);
  if (local_md5.empty()) {
    return false;
  }
  
  // Check remote file info via API
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }
  
  // Build API URL
  std::string protocol = (_config->port == 443 || _config->port == 8443) ? "https" : "http";
  std::string api_url = FormatString("%s://%s:%d/api/fileinfo/%s",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port,
    remote_path.c_str());
  
  // Response buffer
  std::string response_data;
  
  // Set CURL options
  curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Headers
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, ("X-API-Key: " + _config->api_key).c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  
  // Perform request
  CURLcode res = curl_easy_perform(curl);
  
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  
  if (res != CURLE_OK || http_code != 200) {
    return false;
  }
  
  // Parse JSON response
  try {
    rapidjson::Document doc;
    doc.Parse(response_data.c_str());
    
    if (!doc.IsObject() || !doc.HasMember("md5") || !doc.HasMember("exists")) {
      return false;
    }
    
    std::string remote_md5 = doc["md5"].GetString();
    bool exists = doc["exists"].GetBool();
    
    // Compare MD5 hashes
    bool matches = exists && (local_md5 == remote_md5);
    
    return matches;
    
  } catch (const std::exception& e) {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
// S3Uploader
////////////////////////////////////////////////////////////////////////////////

S3Uploader::S3Uploader(s3uploaderconfig_ptr_t config)
    : _config(config) {
  _impl = std::make_unique<Impl>();
}

S3Uploader::~S3Uploader() = default;

bool S3Uploader::uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths) {
  // TODO: Implement batch upload
  for (size_t i = 0; i < local_files.size(); ++i) {
    if (!uploadFile(local_files[i], remote_paths[i])) {
      return false;
    }
  }
  return true;
}

bool S3Uploader::uploadFile(
    const file::Path& local_file,
    const std::string& remote_path) {
  // TODO: Implement S3 upload using AWS SDK
  return false;
}

bool S3Uploader::testConnection() {
  // TODO: Test S3 connection
  return true;
}

bool S3Uploader::remoteFileExists(const std::string& remote_path) {
  // TODO: Check via S3 HEAD request
  return false;
}

bool S3Uploader::deleteRemoteFile(const std::string& remote_path) {
  // TODO: Delete via S3 DELETE
  return false;
}

std::vector<std::string> S3Uploader::listRemoteDirectory(const std::string& path) {
  // TODO: List via S3 API
  return {};
}

void S3Uploader::setBucket(const std::string& bucket) {
  _config->bucket = bucket;
}

void S3Uploader::setRegion(const std::string& region) {
  _config->region = region;
}

void S3Uploader::setStorageClass(const std::string& storage_class) {
  _config->storage_class = storage_class;
}

void S3Uploader::setACL(const std::string& acl) {
  _config->acl = acl;
}

//////////////////////////////////////////////////////////////////////////////
// Helper function to create an SCP uploader with ControlMaster support
//////////////////////////////////////////////////////////////////////////////

uploader_ptr_t createScpUploaderWithControlMaster(
  const std::string& host,
  const std::string& username,
  const std::string& remote_base_path,
  int port
) {
  auto config = std::make_shared<ScpUploaderConfig>();
  config->host = host;
  config->username = username;
  config->port = port;
  config->remote_base_path = remote_base_path;
  config->use_control_master = true;
  // Auto-generate control path if not specified
  // This will use ~/.ssh/cm-<host>-<port>-<user>
  return std::make_shared<ScpUploader>(config);
}

} // namespace ork