////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#if defined(ORK_IOS)

#include <ork/util/download_manager.h>
#include <ork/util/download_group.h>
#include <ork/file/file.h>
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <set>

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
    // Initialize performance logging
    _logchan_download = logger()->configureChannel("DOWNLOAD", fvec3(0.9f, 0.6f, 1.0f), true);
    _perf_timer.Start();
  }

  ~Impl() {
  }

  size_t calculatePendingBytes() {
    std::lock_guard<std::mutex> lock(_download_mutex);
    size_t pending_bytes = 0;

    for (const auto& dl : _pending_downloads) {
      if (dl->_total_bytes > 0) {
        pending_bytes += dl->_total_bytes;
      }
    }

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
      size_t bytes_in_period = _bytes_this_second.exchange(0);
      float bytes_per_sec = bytes_in_period / elapsed;
      size_t pending_bytes = calculatePendingBytes();

      size_t total_downloaded = _total_bytes_downloaded;
      for (const auto& dl : _active_download_set) {
        total_downloaded += dl->_downloaded_bytes;
      }

      _logchan_download->log(
          "totMiB<%g> PendingMiB<%g> MiB/sec<%g> Active<%d> Enqueued<%d> Completed<%d>",
          float(total_downloaded) / 1048576.0f,
          float(pending_bytes) / 1048576.0f,
          bytes_per_sec / 1048576.0f,
          int(_active_downloads.load()),
          int(_queue_size.load()),
          int(_completed_downloads.load()));

      _perf_timer.Start();
    }
  }
};

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
  if (_impl->_shutdown_requested) {
    dl->_state = DownloadState::CANCELLED;
    return;
  }

  dl->_manager_impl = _impl.get();

  {
    std::lock_guard<std::mutex> lock(_impl->_download_mutex);
    _impl->_pending_downloads.insert(dl);
    _impl->_queue_size++;
  }

  _work_queue->enqueue([this, dl]() {
    processDownload(dl);
  });
}

//////////////////////////////////////////////////////////////////////////////

void DownloadManager::downloadGroup(download_group_ptr_t group) {
  for (auto& dl : group->_downloads) {
    enqueue(dl);
  }
}

//////////////////////////////////////////////////////////////////////////////

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

  // iOS: Network downloads not supported
  dl->_state         = DownloadState::FAILED;
  dl->_error_message = "Network downloads not supported on iOS";
  _impl->_failed_downloads++;
  _impl->_queue_size--;
  if (dl->_on_failure._item) {
    dl->_on_failure._item(dl->_error_message);
  }
  updateActiveDownloads();
}

////////////////////////////////////////////////////////////////////////////////

void DownloadManager::updateActiveDownloads() {
  std::lock_guard<std::mutex> lock(_impl->_download_mutex);
  _impl->_active_downloads = _impl->_active_download_set.size();
}

////////////////////////////////////////////////////////////////////////////////

void DownloadManager::shutdown() {
  _impl->_shutdown_requested = true;

  std::lock_guard<std::mutex> lock(_impl->_download_mutex);
  for (auto& dl : _impl->_pending_downloads) {
    dl->_state = DownloadState::CANCELLED;
  }
  for (auto& dl : _impl->_active_download_set) {
    dl->_state = DownloadState::CANCELLED;
  }
  _impl->_pending_downloads.clear();
  _impl->_active_download_set.clear();
}

////////////////////////////////////////////////////////////////////////////////

size_t DownloadManager::activeDownloadCount() const {
  return _impl->_active_downloads;
}

////////////////////////////////////////////////////////////////////////////////

bool DownloadManager::remoteFileExists(
    const URL& url,
    const std::map<std::string, std::string>& headers,
    bool ignore_tls_errors) {
  return false;  // Not supported on iOS
}

////////////////////////////////////////////////////////////////////////////////

chunkverifyresult_vect_t DownloadManager::verifyChunks(
    const URL& verify_url,
    const chunkverifyrequest_vect_t& chunks,
    const std::map<std::string, std::string>& headers,
    bool ignore_tls_errors) {

  chunkverifyresult_vect_t results;

  if (chunks.empty()) {
    return results;
  }

  _impl->_logchan_download->log("iOS: Network verification not supported, returning all as failed");

  // iOS: Network verification not supported, return all as failed
  for (const auto& chunk : chunks) {
    ChunkVerifyResult failed_result;
    failed_result.filename = chunk.filename;
    failed_result.present = false;
    failed_result.hash_ok = false;
    failed_result.error = "Network verification not supported on iOS";
    results.push_back(failed_result);
  }

  return results;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork

#endif // ORK_IOS
