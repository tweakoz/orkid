////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/upload_manager.h>
#include <ork/kernel/opq.h>

namespace ork {

struct UploadManager::Impl {
  std::atomic<size_t> _active_uploads{0};
  std::atomic<bool> _shutdown{false};
};

UploadManager::UploadManager(opq::opq_ptr_t queue)
    : _work_queue(queue)
    , _impl(std::make_unique<Impl>()) {
  if (!_work_queue) {
    _work_queue = opq::concurrentQueue();
  }
}

UploadManager::~UploadManager() {
  shutdown();
}

upload_ptr_t UploadManager::upload(const file::Path& source_path, const URL& dest_url) {
  auto ul = std::make_shared<Upload>();
  ul->_source_path = source_path;
  ul->_destination_url = dest_url;
  
  // Queue upload for processing
  _work_queue->enqueue([this, ul]() {
    processUpload(ul);
  });
  
  return ul;
}

void UploadManager::shutdown() {
  _impl->_shutdown = true;
  // TODO: Wait for active uploads to complete
}

bool UploadManager::isActive() const {
  return _impl->_active_uploads > 0;
}

size_t UploadManager::activeUploadCount() const {
  return _impl->_active_uploads;
}

void UploadManager::processUpload(upload_ptr_t ul) {
  _impl->_active_uploads++;
  
  // Execute the upload
  bool success = ul->execute();
  
  // Handle retry logic if failed
  if (!success && ul->shouldRetry()) {
    ul->_retry_count++;
    // TODO: Implement retry logic
  }
  
  _impl->_active_uploads--;
}



} // namespace ork