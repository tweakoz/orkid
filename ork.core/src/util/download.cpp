////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/download.h>

namespace ork {

////////////////////////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////////////////////////

Download::Download(const URL& _url, const file::Path& dest_path) 
  : _url(_url)
  , _destination_path(dest_path) {
}

////////////////////////////////////////////////////////////////////////////////
// Public Methods
////////////////////////////////////////////////////////////////////////////////

void Download::setHeader(const std::string& key, const std::string& value) {
  _headers[key] = value;
}

////////////////////////////////////////////////////////////////////////////////

void Download::setApiKey(const std::string& key) {
  _api_key = key;
  // Automatically set the X-API-Key header
  setHeader("X-API-Key", key);
}

////////////////////////////////////////////////////////////////////////////////

float Download::progressPercentage() const {
  size_t total = _total_bytes.load();
  if (total == 0) return 0.0f;
  
  size_t downloaded = _downloaded_bytes.load();
  return (float)downloaded / (float)total * 100.0f;
}

////////////////////////////////////////////////////////////////////////////////

bool Download::shouldRetry() const {
  if (_retry_count >= _max_retries) {
    return false;
  }
  
  // Only retry on network errors, not on HTTP errors like 404
  if (_state == DownloadState::FAILED) {
    // TODO: Check specific error types
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////

int Download::getNextRetryDelay() const {
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

////////////////////////////////////////////////////////////////////////////////
} // namespace ork