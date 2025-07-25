////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/download_manager.h>
#include <ork/util/download_group.h>
#include <ork/file/file.h>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curl/curl.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <set>
#include <fstream>

namespace ork {

////////////////////////////////////////////////////////////////////////////////

struct DownloadManager::Impl {
  std::atomic<bool> _shutdown_requested{false};
  std::atomic<size_t> _active_downloads{0};
  std::mutex _download_mutex;
  std::set<download_ptr_t> _pending_downloads;
  std::set<download_ptr_t> _active_download_set;
  
  Impl() {
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_ALL);
  }
  
  ~Impl() {
    // Cleanup curl
    curl_global_cleanup();
  }
};

//////////////////////////////////////////////////////////////////////////////
// CURL Callbacks
//////////////////////////////////////////////////////////////////////////////

// CURL callback for writing data
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  auto* stream = static_cast<std::ofstream*>(userp);
  size_t total_size = size * nmemb;
  stream->write(static_cast<const char*>(contents), total_size);
  return total_size;
}

// CURL callback for progress
static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
  auto* download = static_cast<Download*>(clientp);
  
  download->_total_bytes = static_cast<size_t>(dltotal);
  download->_downloaded_bytes = static_cast<size_t>(dlnow);
  
  if (download->_on_progress._item && dltotal > 0) {
    download->_on_progress._item(static_cast<size_t>(dlnow), static_cast<size_t>(dltotal));
  }
  
  // Return 0 to continue, non-zero to abort
  return (download->_state == DownloadState::CANCELLED) ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////////

DownloadManager::DownloadManager(opq::opq_ptr_t queue) 
  : _work_queue(queue ? queue : opq::concurrentQueue())
  , _impl(std::make_unique<Impl>()) {
}

//////////////////////////////////////////////////////////////////////////////

DownloadManager::~DownloadManager() {
  shutdown();
}

//////////////////////////////////////////////////////////////////////////////

download_ptr_t DownloadManager::download(const URL& url, const file::Path& dest_path) {
  auto dl = std::make_shared<Download>(url, dest_path);
  
  // Queue the download for processing
  _work_queue->enqueue([this, dl]() {
    processDownload(dl);
  });
  
  {
    std::lock_guard<std::mutex> lock(_impl->_download_mutex);
    _impl->_pending_downloads.insert(dl);
  }
  
  return dl;
}

//////////////////////////////////////////////////////////////////////////////

void DownloadManager::downloadGroup(download_group_ptr_t group) {
  // Process all downloads in the group
  for (auto& dl : group->_downloads) {
    std::lock_guard<std::mutex> lock(_impl->_download_mutex);
    _impl->_pending_downloads.insert(dl);
  }
  for (auto& dl : group->_downloads) {
    _work_queue->enqueue([this, dl, group]() {
      processDownload(dl);
      group->checkCompletion();
    });
    
  }
}

////////////////////////////////////////////////////////////////////////////////

void DownloadManager::processDownload(download_ptr_t dl) {
  // Wait if we're at max concurrent downloads
  while (_impl->_active_downloads >= _max_concurrent_downloads && !_impl->_shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  if (_impl->_shutdown_requested) {
    dl->_state = DownloadState::CANCELLED;
    return;
  }
  
  // Move from pending to active
  {
    std::lock_guard<std::mutex> lock(_impl->_download_mutex);
    _impl->_pending_downloads.erase(dl);
    _impl->_active_download_set.insert(dl);
    _impl->_active_downloads++;
  }
  
  dl->_state = DownloadState::DOWNLOADING;
  
  // Create parent directory if needed
  // Create parent directory if needed
  auto parent_path = dl->_destination_path.toAbsolute();
  // TODO: Add directory creation logic
  
  // Setup CURL
  CURL* curl = curl_easy_init();
  if (!curl) {
    dl->_state = DownloadState::FAILED;
    dl->_error_message = "Failed to initialize CURL";
    if (dl->_on_failure._item) {
      dl->_on_failure._item(dl->_error_message);
    }
    updateActiveDownloads();
    return;
  }
  
  dl->_curl_handle = curl;
  
  // Open output file
  std::ofstream output_file(dl->_destination_path.c_str(), std::ios::binary);
  if (!output_file.is_open()) {
    dl->_state = DownloadState::FAILED;
    dl->_error_message = std::string("Failed to open output file: ") + dl->_destination_path.c_str();
    if (dl->_on_failure._item) {
      dl->_on_failure._item(dl->_error_message);
    }
    curl_easy_cleanup(curl);
    updateActiveDownloads();
    return;
  }
  
  ///////////////////////////////////////////////////////////
  // Configure CURL options
  ///////////////////////////////////////////////////////////
  curl_easy_setopt(curl, CURLOPT_URL, dl->_url.toString().c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_file);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, dl.get());
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);  // No timeout
  
  // Enable verbose debug output
  //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  
  // Log the URL being downloaded
  printf("[CURL] Downloading from: %s\n", dl->_url.toString().c_str());
  
  ///////////////////////////////////////////////////////////
  // Handle TLS options
  ///////////////////////////////////////////////////////////
  if (dl->_ignore_tls_errors) {
    printf("[CURL] Disabling TLS certificate verification\n");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  } else {
    printf("[CURL] TLS certificate verification enabled\n");
  }
  
  ///////////////////////////////////////////////////////////
  // Set headers
  ///////////////////////////////////////////////////////////
  struct curl_slist* headers = nullptr;
  printf("[CURL] Setting headers:\n");
  for (const auto& [key, value] : dl->_headers) {
    std::string header = key + ": " + value;
    // Log header (mask API key value for security)
    if (key == "X-API-Key" || key == "Authorization") {
      printf("  %s: ***masked***\n", key.c_str());
    } else {
      printf("  %s: %s\n", key.c_str(), value.c_str());
    }
    headers = curl_slist_append(headers, header.c_str());
  }
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  } else {
    printf("  (no custom headers)\n");
  }
  
  ///////////////////////////////////////////////////////////
  // Perform the download
  ///////////////////////////////////////////////////////////
  CURLcode res = curl_easy_perform(curl);
  
  ///////////////////////////////////////////////////////////
  // Cleanup
  ///////////////////////////////////////////////////////////
  output_file.close();
  if (headers) {
    curl_slist_free_all(headers);
  }
  curl_easy_cleanup(curl);
  dl->_curl_handle = nullptr;
  
  ///////////////////////////////////////////////////////////
  // Handle result
  ///////////////////////////////////////////////////////////
  if (res == CURLE_OK) {
    // Get HTTP response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    printf("[CURL] HTTP Response Code: %ld\n", response_code);
    
    if (response_code >= 200 && response_code < 300) {
      dl->_state = DownloadState::COMPLETED;
      if (dl->_on_complete._item) {
        dl->_on_complete._item(true, dl->_destination_path);
      }
    } else {
      dl->_state = DownloadState::FAILED;
      dl->_error_message = "HTTP error " + std::to_string(response_code);
      printf("[CURL] Download failed with HTTP %ld\n", response_code);
      if (dl->_on_failure._item) {
        dl->_on_failure._item(dl->_error_message);
      }
      // Remove partial file
      std::remove(dl->_destination_path.c_str());
    }
  } else {
    dl->_state = DownloadState::FAILED;
    dl->_error_message = curl_easy_strerror(res);
    printf("[CURL] Download failed: %s\n", dl->_error_message.c_str());
    if (dl->_on_failure._item) {
      dl->_on_failure._item(dl->_error_message);
    }
    // Remove partial file
    std::remove(dl->_destination_path.c_str());
  }
  
  updateActiveDownloads();
}

////////////////////////////////////////////////////////////////////////////////

void DownloadManager::updateActiveDownloads() {
  std::lock_guard<std::mutex> lock(_impl->_download_mutex);
  
  // Remove completed downloads from active set
  auto it = _impl->_active_download_set.begin();
  while (it != _impl->_active_download_set.end()) {
    auto dl = *it;
    if (dl->_state == DownloadState::COMPLETED || 
        dl->_state == DownloadState::FAILED ||
        dl->_state == DownloadState::CANCELLED) {
      it = _impl->_active_download_set.erase(it);
      _impl->_active_downloads--;
    } else {
      ++it;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void DownloadManager::setMaxConcurrentDownloads(size_t max) {
  _max_concurrent_downloads = max;
}

////////////////////////////////////////////////////////////////////////////////

void DownloadManager::shutdown() {
  _impl->_shutdown_requested = true;
  
  // Cancel all pending downloads
  {
    std::lock_guard<std::mutex> lock(_impl->_download_mutex);
    for (auto& dl : _impl->_pending_downloads) {
      dl->_state = DownloadState::CANCELLED;
    }
    for (auto& dl : _impl->_active_download_set) {
      dl->_state = DownloadState::CANCELLED;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

bool DownloadManager::isActive() const {
  return _impl->_active_downloads > 0 || !_impl->_pending_downloads.empty();
}

////////////////////////////////////////////////////////////////////////////////

size_t DownloadManager::activeDownloadCount() const {
  return _impl->_active_downloads;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork