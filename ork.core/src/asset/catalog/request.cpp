////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/packager.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/request.h>
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

////////////////////////////////////////////////////////////////////////////////

AssetHandle::AssetHandle() {
}

AssetHandle::AssetHandle(const std::string& ns) 
  : _namespace(ns) {
}

AssetHandle::AssetHandle(const std::string& ns, const std::string& _asset_id)
  : _namespace(ns)
  , _asset_id(_asset_id) {
}

bool AssetHandle::isValid() const {
  return !_namespace.empty();
}

////////////////////////////////////////////////////////////////
// AssetFuture Implementation
////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetFuture::wait() {
  std::unique_lock<std::mutex> lock(_mutex);
  _cv.wait(lock, [this] { return _is_complete.load() || _is_cancelled.load(); });
  
  if (_is_cancelled) {
    if (!_result) {
      _result = std::make_shared<AssetResult>();
      _result->_status = AssetStatus::CANCELLED;
      _result->_error_detail = "Operation was cancelled";
    }
  }
  
  return _result;
}

void AssetFuture::cancel() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _is_cancelled = true;
    
    // If not already complete, create a cancelled result
    if (!_is_complete) {
      _result = std::make_shared<AssetResult>();
      _result->_status = AssetStatus::CANCELLED;
      _result->_error_detail = "Operation was cancelled";
      _is_complete = true;
    }
  }
  _cv.notify_all();
}

assetresult_ptr_t AssetFuture::getResult() const {
  if (_is_complete.load()) {
    return _result;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////
// AssetResult implementations moved from header
////////////////////////////////////////////////////////////////

bool AssetResult::isSuccess() const {
  return _status == AssetStatus::OK;
}

AssetResult::operator bool() const {
  return isSuccess();
}

} // namespace ork::asset::catalog {
