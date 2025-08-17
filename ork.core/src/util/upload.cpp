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

////////////////////////////////////////////////////////////////

struct Upload::Impl {
  // TODO: Add implementation details
};

////////////////////////////////////////////////////////////////

Upload::Upload()
    : _impl(std::make_unique<Impl>()) {
}

////////////////////////////////////////////////////////////////

Upload::~Upload() {
}

////////////////////////////////////////////////////////////////

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
  // TODO: Add SCP and S3 support when implemented
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

////////////////////////////////////////////////////////////////

void Upload::cancel() {
  _state = UploadState::CANCELLED;
}

////////////////////////////////////////////////////////////////

float Upload::getProgress() const {
  if (_total_bytes == 0) {
    return 0.0f;
  }
  return static_cast<float>(_bytes_uploaded) / static_cast<float>(_total_bytes);
}

////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////

bool Upload::shouldRetry() const {
  if (_retry_count >= _max_retries) {
    return false;
  }
  
  // TODO: Implement error-specific retry logic
  // For now, always retry on failure unless cancelled
  return _state == UploadState::FAILED;
}

////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Uploader base class
////////////////////////////////////////////////////////////////////////////////

// Destructor is already defined as default in the header


} // namespace ork