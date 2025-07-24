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
#include <ork/util/download.h>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace ork {

struct DownloadGroup;
using download_group_ptr_t = std::shared_ptr<DownloadGroup>;

// Group progress callback: (completed_count, total_count) -> void
using group_progress_fn_t = std::function<void(size_t, size_t)>;

// Group completion callback: (all_success) -> void
using group_complete_fn_t = std::function<void(bool)>;

// Item state change callback: (download, old_state, new_state) -> void
using group_item_state_fn_t = std::function<void(download_ptr_t, DownloadState, DownloadState)>;

struct DownloadGroup {
  //////////////////////////////////////////////////////////////////////////////
  // Public members
  //////////////////////////////////////////////////////////////////////////////
  std::vector<download_ptr_t> _downloads;
  std::atomic<size_t> _completed_count{0};
  std::atomic<size_t> _failed_count{0};
  
  //////////////////////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////////////////////
  ItemAndData<group_progress_fn_t> _on_progress;
  ItemAndData<group_complete_fn_t> _on_complete;
  ItemAndData<group_item_state_fn_t> _on_item_state_change;
  
  // Constructor
  DownloadGroup() = default;
  
  // Methods
  download_ptr_t addDownload(const URL& url, const file::Path& dest_path);
  void addDownload(download_ptr_t dl);
  
  // State queries
  bool isComplete() const;
  bool allSuccessful() const;
  size_t totalCount() const { return _downloads.size(); }
  float progressPercentage() const;
  
  // Internal use
  void checkCompletion();
  
private:
  mutable std::mutex _mutex;
  std::atomic<bool> _completion_notified{false};
  
  void setupDownloadCallbacks(download_ptr_t dl);
};

} // namespace ork