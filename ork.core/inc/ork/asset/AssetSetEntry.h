///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/asset/Asset.h>
#include <ork/asset/AssetSet.h>
#include <ork/asset/AssetLoader.h>
#include <ork/util/dependency/Provider.h>

namespace ork { namespace asset {

class AssetDependent;
class AssetSetLevel;

class AssetSetEntry {
public:
  AssetSetEntry(asset_ptr_t asset, AssetLoader* loader, AssetSetLevel* level);
  ~AssetSetEntry();

#if defined(ORKCONFIG_ASSET_UNLOAD)
  bool unload(AssetSetLevel* level);
#endif
  bool Load(AssetSetLevel* level);
  void OnPush(AssetSetLevel* level);
  void OnPop(AssetSetLevel* level);
  asset_ptr_t asset() const;
  AssetLoader* GetLoader() const;
  bool IsLoaded();
  util::dependency::Provider* GetLoadProvider();

private:
  asset_ptr_t _asset;
  AssetLoader* mLoader;
  util::dependency::Provider mLoadProvider;
  AssetSetLevel* mDeclareLevel;
  AssetSetLevel* mLoadLevel;
};

AssetSetEntry* assetSetEntry(const Asset* asset);

}} // namespace ork::asset
