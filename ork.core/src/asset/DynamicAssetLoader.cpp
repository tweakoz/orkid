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
namespace ork { namespace asset {
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

bool DynamicAssetLoader::CheckAsset(const PieceString& name) {
  return (mCheckFn != nullptr) ? mCheckFn(name) : false;
}

///////////////////////////////////////////////////////////////////////////////

bool DynamicAssetLoader::LoadAsset(asset_ptr_t asset) {
  return (mLoadFn != nullptr) ? mLoadFn(asset) : false;
}

void DynamicAssetLoader::DestroyAsset(asset_ptr_t asset) {
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::asset
///////////////////////////////////////////////////////////////////////////////
