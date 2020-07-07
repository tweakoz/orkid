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
    : mCheckFn(nullptr)
    , mLoadFn(nullptr)
    , mEnumFn(nullptr) {
}

std::set<file::Path> DynamicAssetLoader::EnumerateExisting() {
  std::set<file::Path> rval;
  if (mEnumFn)
    rval = mEnumFn();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool DynamicAssetLoader::doesExist(const AssetPath& name) {
  return (mCheckFn != nullptr) ? mCheckFn(name) : false;
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
  asset_ptr_t loaded = (mLoadFn != nullptr) ? mLoadFn(name, vars) : nullptr;
  return loaded;
}

void DynamicAssetLoader::destroy(asset_ptr_t asset) {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
