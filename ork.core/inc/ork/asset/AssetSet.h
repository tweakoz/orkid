///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/asset/Asset.h>

namespace ork { namespace asset {

class AssetClass;
class AssetEntry;
class AssetLoader;
class AssetSetLevel;
class AssetSetEntry;

class AssetSet {
public:
  static void Describe();

  AssetSet();

  void Register(PoolString name, asset_ptr_t asset, AssetLoader* loader = NULL);
  asset_ptr_t FindAsset(PoolString name);
  AssetSetEntry* FindAssetEntry(PoolString name);
  AssetLoader* FindLoader(PoolString name);

  bool Load(int depth = -1);
#if defined(ORKCONFIG_ASSET_UNLOAD)
  bool UnLoad(int depth = -1);
#endif

  AssetSetLevel* GetTopLevel() const;

  void PushLevel(AssetClass*);
  void PopLevel();

private:
  AssetSetLevel* mTopLevel;
};

}} // namespace ork::asset
