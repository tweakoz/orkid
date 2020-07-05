////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/asset/Asset.h>
#include <ork/asset/AssetSetEntry.h>
#include <ork/asset/AssetDependent.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

AssetSetEntry::AssetSetEntry(
    asset_ptr_t asset, //
    AssetLoader* loader,
    AssetSetLevel* level)
    : _asset(asset)
    , mLoader(loader)
    , mDeclareLevel(level)
    , mLoadLevel(NULL) {
}

///////////////////////////////////////////////////////////////////////////////

bool AssetSetEntry::Load(AssetSetLevel* level) {
  /*if (NULL == mLoadLevel) {
    if (NULL == mLoader || false == mLoader->load(_asset)) {
      mLoadLevel = NULL;

      return false;
    }

    mLoadLevel = level;

    mLoadProvider.Provide();
  }
*/
  return true;
}

///////////////////////////////////////////////////////////////////////////////

#if defined(ORKCONFIG_ASSET_UNLOAD)
bool AssetSetEntry::unload(AssetSetLevel* level) {
  if (IsLoaded()) {
    if (mLoader) {
      mLoader->DestroyAsset(_asset);
      mLoadProvider.Revoke();
      mLoadLevel = NULL;
      return true;
    }
  }

  return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////

void AssetSetEntry::OnPush(AssetSetLevel* level) {
  OrkAssert(_asset);
}

///////////////////////////////////////////////////////////////////////////////

void AssetSetEntry::OnPop(AssetSetLevel* level) {
  OrkAssert(_asset);

  if (mLoadLevel == level) {
    mLoadProvider.Revoke();
    mLoader->destroy(_asset);

    mLoadLevel = NULL;
  }

  if (mDeclareLevel == level) {
    OrkAssert(false == mLoadProvider.Providing());
    OrkAssert(NULL == mLoadLevel);

    this->~AssetSetEntry();
  }
}

///////////////////////////////////////////////////////////////////////////////

AssetSetEntry::~AssetSetEntry() {
}

///////////////////////////////////////////////////////////////////////////////

asset_ptr_t AssetSetEntry::asset() const {
  return _asset;
}

///////////////////////////////////////////////////////////////////////////////

AssetLoader* AssetSetEntry::GetLoader() const {
  return mLoader;
}

///////////////////////////////////////////////////////////////////////////////

bool AssetSetEntry::IsLoaded() {
  return 0 != mLoadLevel;
}

///////////////////////////////////////////////////////////////////////////////

util::dependency::Provider* AssetSetEntry::GetLoadProvider() {
  return &mLoadProvider;
}

///////////////////////////////////////////////////////////////////////////////

AssetSetEntry* assetSetEntry(const Asset* asset) {
  auto asset_set = asset->assetSet();
  auto entry     = asset_set->FindAssetEntry(asset->name());

  return entry;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::asset
