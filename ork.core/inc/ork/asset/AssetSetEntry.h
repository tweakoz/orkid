////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/asset/AssetSet.h>
#include <ork/asset/AssetLoader.h>
#include <ork/util/dependency/Provider.h>

namespace ork { namespace asset {

struct AssetDependent;
struct AssetSetLevel;

struct AssetSetEntry {
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
