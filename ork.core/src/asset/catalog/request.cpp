////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/request.h>
#include <ork/util/download_group.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/mutex.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <sstream>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////
// AssetFuture Implementation
////////////////////////////////////////////////////////////////

bool FetchRequest::wait() {
  while( not isComplete() ) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  return isSuccess();
}

////////////////////////////////////////////////////////////////

bool FetchRequest::isComplete() const {
  switch(_state) {
    case AssetState::NEW:               // brand new request
    case AssetState::ENQUEUE_PENDING:   // merged but not yet queued
    case AssetState::ENQUEUED:          // waiting to be downloaded
    case AssetState::DOWNLOADING:       // downloading in progress
    case AssetState::PROCESSING:       // downloading in progress
      return false;
    case AssetState::SUCCEEDED:
      //printf("[DEBUG] FetchRequest::isComplete() SUCCEEDED\n");
      return true;
    case AssetState::FAILED:
      printf("[DEBUG] FetchRequest::isComplete() FAILED\n");
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////
// AssetResult implementations moved from header
////////////////////////////////////////////////////////////////

bool FetchRequest::isSuccess() const {
  return _state == AssetState::SUCCEEDED;
}

FetchRequest::operator bool() const {
  return isSuccess();
}

void FetchRequest::cancel() {
  _cancel_requested = true;
  // Propagate cancel to all active downloads
  _active_download_group.atomicOp([](download_group_ptr_t& group) {
    if (group) {
      for (auto& dl : group->_downloads) {
        dl->_state = DownloadState::CANCELLED;
      }
    }
  });
}

void FetchRequest::invokeCompletionCallbacks(fetchrequest_ptr_t self) { // static
  asset_callback_list_t callbacks; 
  self->_completion_callbacks.atomicOp([&](const asset_callback_list_t& list) {
    callbacks = list; // copy to avoid holding lock during callbacks
  });
  for (const auto& cb : callbacks) {
    if (cb) {
      cb(self);
    }
  }
}
} // namespace ork::asset::catalog {
