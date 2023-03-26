////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/string/string.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::asset {
///////////////////////////////////////////////////////////////////////////////

DynamicAssetLoader::DynamicAssetLoader()
    : _checkFn(nullptr)
    , _loadFn(nullptr)
    , _enumFn(nullptr) {
}

std::set<file::Path> DynamicAssetLoader::EnumerateExisting() {
  std::set<file::Path> rval;
  if (_enumFn)
    rval = _enumFn();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool DynamicAssetLoader::doesExist(const AssetPath& name) {
  return (_checkFn != nullptr) ? _checkFn(name) : false;
}

bool DynamicAssetLoader::resolvePath(
    const AssetPath& pathin,    //
    AssetPath& resolved_path) { // override
  auto found = doesExist(pathin);
  if (found)
    resolved_path = pathin;
  return found;
}

///////////////////////////////////////////////////////////////////////////////

asset_ptr_t DynamicAssetLoader::load(loadrequest_ptr_t loadreq) {
  loadreq->incrementPartialLoadCount();
  asset_ptr_t loaded = (_loadFn != nullptr) ? _loadFn(loadreq) : nullptr;
  if (loaded){
    loaded->_load_request = loadreq;
    loaded->_name = loadreq->_asset_path;
    loadreq->_asset = loaded;
  }
  loadreq->decrementPartialLoadCount();
  return loaded;
}

void DynamicAssetLoader::destroy(asset_ptr_t asset) {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
