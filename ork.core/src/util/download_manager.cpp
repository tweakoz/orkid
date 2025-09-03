////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/download_manager.h>
#include <ork/util/download_group.h>
#include <ork/file/file.h>
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
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

  // Performance tracking
  Timer _perf_timer;
  logchannel_ptr_t _logchan_download;

  // Statistics
  std::atomic<size_t> _total_bytes_downloaded{0};
  std::atomic<size_t> _bytes_this_second{0};
  std::atomic<int> _completed_downloads{0};
  std::atomic<int> _failed_downloads{0};
  std::atomic<size_t> _queue_size{0};

  Impl() {
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_ALL);

    // Initialize performance logging
    _logchan_download = logger()->configureChannel("DOWNLOAD", fvec3(0.9f, 0.6f, 1.0f), true); // Light blue
    _perf_timer.Start();
  }

  ~Impl() {
    // Cleanup curl
    curl_global_cleanup();
  }

  size_t calculatePendingBytes() {
    std::lock_guard<std::mutex> lock(_download_mutex);
    size_t pending_bytes = 0;

    // Sum up total bytes from pending downloads
    for (const auto& dl : _pending_downloads) {
      // Only count if we know the size (total_bytes > 0)
      if (dl->_total_bytes > 0) {
        pending_bytes += dl->_total_bytes;
      }
    }

    // Also add remaining bytes from active downloads
    for (const auto& dl : _active_download_set) {
      if (dl->_total_bytes > 0 && dl->_total_bytes > dl->_downloaded_bytes) {
        pending_bytes += (dl->_total_bytes - dl->_downloaded_bytes);
      }
    }

    return pending_bytes;
  }

  void emitPerfMetrics() {
    float elapsed = _perf_timer.SecsSinceStart();
    if (elapsed >= 1.0f) {
      // Calculate bytes per second over the actual elapsed time
      size_t bytes_in_period = _bytes_this_second.exchange(0);
      float bytes_per_sec = bytes_in_period / elapsed;
      size_t pending_bytes = calculatePendingBytes();
      
      // Calculate total downloaded from all active downloads
      size_t total_downloaded = _total_bytes_downloaded;
      for (const auto& dl : _active_download_set) {
        total_downloaded += dl->_downloaded_bytes;
      }

      _logchan_download->log(
          "totMiB<%g> PendingMiB<%g> MiB/sec<%g> Active<%d> Enqueued<%d> Completed<%d>", //
          float(total_downloaded) / 1048576.0f,                                    //
          float(pending_bytes) / 1048576.0f,
          bytes_per_sec / 1048576.0f, //
          int(_active_downloads.load()),
          int(_queue_size.load()),
          int(_completed_downloads.load()));

      // Reset timer
      _perf_timer.Start();
    }
  }
};

//////////////////////////////////////////////////////////////////////////////
// CURL Callbacks
//////////////////////////////////////////////////////////////////////////////

// CURL callback for writing data
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  auto* stream      = static_cast<std::ofstream*>(userp);
  size_t total_size = size * nmemb;
  stream->write(static_cast<const char*>(contents), total_size);
  return total_size;
}

// CURL callback for progress
static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
  auto* download = static_cast<Download*>(clientp);

  // Track bytes downloaded this callback
  size_t prev_bytes           = download->_downloaded_bytes;
  download->_total_bytes      = static_cast<size_t>(dltotal);
  download->_downloaded_bytes = static_cast<size_t>(dlnow);

  // Update download manager stats if we have one
  if (download->_manager_impl) {
    size_t bytes_delta = (dlnow > prev_bytes) ? (dlnow - prev_bytes) : 0;
    auto* impl         = static_cast<DownloadManager::Impl*>(download->_manager_impl);
    impl->_bytes_this_second += bytes_delta;
    // Don't update _total_bytes_downloaded here - only when downloads complete

    // Emit performance metrics (throttled to 1Hz)
    impl->emitPerfMetrics();
  }

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
  enqueue(dl);
  return dl;
}

//////////////////////////////////////////////////////////////////////////////
void DownloadManager::enqueue(download_ptr_t dl) {
  // Set manager impl for performance tracking
  dl->_manager_impl = _impl.get();

  // Update queue size
  _impl->_queue_size++;

  // Queue the download for processing
  _work_queue->enqueue([this, dl]() { processDownload(dl); });

  {
    std::lock_guard<std::mutex> lock(_impl->_download_mutex);
    _impl->_pending_downloads.insert(dl);
  }
}

//////////////////////////////////////////////////////////////////////////////

void DownloadManager::downloadGroup(download_group_ptr_t group) {
  // Process all downloads in the group
  for (auto& dl : group->_downloads) {
    // Set manager impl for performance tracking
    dl->_manager_impl = _impl.get();

    // Update queue size
    _impl->_queue_size++;

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
  
  if (_impl->_shutdown_requested) {
    dl->_state = DownloadState::CANCELLED;
    _impl->_failed_downloads++;
    _impl->_queue_size--;
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
    dl->_state         = DownloadState::FAILED;
    dl->_error_message = "Failed to initialize CURL";
    _impl->_failed_downloads++;
    _impl->_queue_size--;
    if (dl->_on_failure._item) {
      dl->_on_failure._item(dl->_error_message);
    }
    updateActiveDownloads();
    return;
  }

  // Don't store CURL handle in shared object to avoid thread safety issues

  // Open output file
  std::ofstream output_file(dl->_destination_path.c_str(), std::ios::binary);
  if (!output_file.is_open()) {
    dl->_state         = DownloadState::FAILED;
    dl->_error_message = std::string("Failed to open output file: ") + dl->_destination_path.c_str();
    _impl->_failed_downloads++;
    _impl->_queue_size--;
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
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // No timeout

  // Enable verbose debug output
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  // Log the URL being downloaded
  if(0)_impl->_logchan_download->log(
      "DL %s -> %s",               //
      dl->_url.toString().c_str(), //
      dl->_destination_path.c_str());

  ///////////////////////////////////////////////////////////
  // Handle TLS options
  ///////////////////////////////////////////////////////////

  if (dl->_ignore_tls_errors) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  } else {
  }

  ///////////////////////////////////////////////////////////
  // Set headers
  ///////////////////////////////////////////////////////////
  struct curl_slist* headers = nullptr;
  for (const auto& [key, value] : dl->_headers) {
    std::string header = key + ": " + value;
    // Log header (mask API key value for security)
    headers = curl_slist_append(headers, header.c_str());
  }
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  } else {
    // printf("  (no custom headers)\n");
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

  ///////////////////////////////////////////////////////////
  // Handle result
  ///////////////////////////////////////////////////////////
  if (res == CURLE_OK) {
    // Get HTTP response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code >= 200 && response_code < 300) {
      dl->_state = DownloadState::COMPLETED;
      _impl->_completed_downloads++;
      _impl->_total_bytes_downloaded += dl->_downloaded_bytes;
      if (dl->_on_complete._item) {
        dl->_on_complete._item(true, dl->_destination_path);
      }
    } else {
      dl->_state         = DownloadState::FAILED;
      dl->_error_message = "HTTP error " + std::to_string(response_code);
      _impl->_logchan_download->log("Download failed with HTTP %ld", response_code);
      _impl->_logchan_download->log("  url=%s", dl->_url.toString().c_str());
      _impl->_failed_downloads++;
      if (dl->_on_failure._item) {
        dl->_on_failure._item(dl->_error_message);
      }
      // Remove partial file
      std::remove(dl->_destination_path.c_str());
    }
  } else {
    dl->_state         = DownloadState::FAILED;
    dl->_error_message = curl_easy_strerror(res);
    _impl->_logchan_download->log("Download failed: %s", dl->_error_message.c_str());
    _impl->_logchan_download->log("  url=%s", dl->_url.toString().c_str());
    _impl->_failed_downloads++;
    if (dl->_on_failure._item) {
      dl->_on_failure._item(dl->_error_message);
    }
    // Remove partial file
    std::remove(dl->_destination_path.c_str());
  }

  // Cleanup curl handle
  curl_easy_cleanup(curl);

  // Update queue size when download completes
  _impl->_queue_size--;

  // Emit final performance metrics
  _impl->emitPerfMetrics();

  updateActiveDownloads();
}

////////////////////////////////////////////////////////////////////////////////

void DownloadManager::updateActiveDownloads() {
  std::lock_guard<std::mutex> lock(_impl->_download_mutex);

  // Remove completed downloads from active set
  auto it = _impl->_active_download_set.begin();
  while (it != _impl->_active_download_set.end()) {
    auto dl = *it;
    if (dl->_state == DownloadState::COMPLETED || dl->_state == DownloadState::FAILED || dl->_state == DownloadState::CANCELLED) {
      it = _impl->_active_download_set.erase(it);
      _impl->_active_downloads--;
    } else {
      ++it;
    }
  }
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

bool DownloadManager::remoteFileExists(const URL& url, const std::map<std::string, std::string>& headers, bool ignore_tls_errors) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, url.toString().c_str());

  // Use HEAD method
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(curl, CURLOPT_HEADER, 0L);

  // Follow redirects
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  // TLS options
  if (true) { //ignore_tls_errors) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  // Set headers
  struct curl_slist* curl_headers = nullptr;
  for (const auto& [key, value] : headers) {
    std::string header = key + ": " + value;
    curl_headers       = curl_slist_append(curl_headers, header.c_str());
  }
  if (curl_headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  }

  // Perform the request
  CURLcode res = curl_easy_perform(curl);

  bool exists = false;
  if (res == CURLE_OK) {
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    exists = (response_code == 200);

    // Log result if we have the download channel
    if (_impl->_logchan_download) {
      _impl->_logchan_download->log("HEAD %s -> HTTP %ld", url.toString().c_str(), response_code);
    }
  }

  // Cleanup
  if (curl_headers) {
    curl_slist_free_all(curl_headers);
  }
  curl_easy_cleanup(curl);

  return exists;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork