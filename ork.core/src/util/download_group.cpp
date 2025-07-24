////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/download_group.h>

namespace ork {

////////////////////////////////////////////////////////////////////////////////

download_ptr_t DownloadGroup::addDownload(const URL& url, const file::Path& dest_path) {
  auto dl = std::make_shared<Download>(url, dest_path);
  addDownload(dl);
  return dl;
}

////////////////////////////////////////////////////////////////////////////////

void DownloadGroup::addDownload(download_ptr_t dl) {
  std::lock_guard<std::mutex> lock(_mutex);
  
  _downloads.push_back(dl);
  setupDownloadCallbacks(dl);
}

////////////////////////////////////////////////////////////////////////////////

void DownloadGroup::setupDownloadCallbacks(download_ptr_t dl) {
  // Store original callbacks
  auto orig_complete = dl->_on_complete._item;
  auto orig_failure = dl->_on_failure._item;
  auto orig_progress = dl->_on_progress._item;
  
  // Wrap completion callback
  dl->_on_complete._item = [this, dl, orig_complete](bool success, const file::Path& path) {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _completed_count++;
      
      if (_on_item_state_change._item) {
        _on_item_state_change._item(dl, DownloadState::DOWNLOADING, DownloadState::COMPLETED);
      }
      
      if (_on_progress._item) {
        _on_progress._item(_completed_count + _failed_count, _downloads.size());
      }
    }
    
    // Call original callback
    if (orig_complete) {
      orig_complete(success, path);
    }
    
    // Check if group is complete
    checkCompletion();
  };
  
  // Wrap failure callback
  dl->_on_failure._item = [this, dl, orig_failure](const std::string& error) {
    {
      std::lock_guard<std::mutex> lock(_mutex);
      _failed_count++;
      
      if (_on_item_state_change._item) {
        _on_item_state_change._item(dl, DownloadState::DOWNLOADING, DownloadState::FAILED);
      }
      
      if (_on_progress._item) {
        _on_progress._item(_completed_count + _failed_count, _downloads.size());
      }
    }
    
    // Call original callback
    if (orig_failure) {
      orig_failure(error);
    }
    
    // Check if group is complete
    checkCompletion();
  };
  
  // Wrap progress callback if we don't have one
  if (!dl->_on_progress._item && orig_progress) {
    dl->_on_progress._item = orig_progress;
  }
}

////////////////////////////////////////////////////////////////////////////////

bool DownloadGroup::isComplete() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return (_completed_count + _failed_count) >= _downloads.size();
}

////////////////////////////////////////////////////////////////////////////////

bool DownloadGroup::allSuccessful() const {
  std::lock_guard<std::mutex> lock(_mutex);
  return _failed_count == 0 && _completed_count == _downloads.size();
}

////////////////////////////////////////////////////////////////////////////////

float DownloadGroup::progressPercentage() const {
  std::lock_guard<std::mutex> lock(_mutex);
  if (_downloads.empty()) return 100.0f;
  
  size_t total_downloaded = 0;
  size_t total_size = 0;
  
  for (const auto& dl : _downloads) {
    total_downloaded += dl->_downloaded_bytes;
    total_size += dl->_total_bytes;
  }
  
  if (total_size == 0) {
    // Fall back to counting completed items
    return (float)(_completed_count + _failed_count) / (float)_downloads.size() * 100.0f;
  }
  
  return (float)total_downloaded / (float)total_size * 100.0f;
}

////////////////////////////////////////////////////////////////////////////////

void DownloadGroup::checkCompletion() {
  bool should_notify = false;
  bool all_success = false;
  
  {
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Check if complete without calling isComplete() which tries to lock again
    bool complete = (_completed_count + _failed_count) >= _downloads.size();
    
    if (complete && !_completion_notified) {
      _completion_notified = true;
      should_notify = true;
      all_success = (_failed_count == 0 && _completed_count == _downloads.size());
    }
  }
  
  // Call completion callback outside of lock
  if (should_notify && _on_complete._item) {
    _on_complete._item(all_success);
  }
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork