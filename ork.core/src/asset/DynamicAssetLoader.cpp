////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

asset_ptr_t DynamicAssetLoader::load(const AssetPath& name, vars_constptr_t vars) {
  asset_ptr_t loaded = (_loadFn != nullptr) ? _loadFn(name, vars) : nullptr;
  if (loaded)
    loaded->_name = name;
  return loaded;
}

void DynamicAssetLoader::destroy(asset_ptr_t asset) {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
