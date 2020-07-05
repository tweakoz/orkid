///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/asset/Asset.h>

namespace ork { namespace asset {

class AssetEntry;
class AssetLoader;
class AssetSetLevel;
class AssetSetEntry;

class AssetSet {
public:
  static void Describe();

  AssetSet();

  void Register(AssetPath name, asset_ptr_t asset, AssetLoader* loader = NULL);
  asset_ptr_t FindAsset(AssetPath name);
  AssetSetEntry* FindAssetEntry(AssetPath name);
  AssetLoader* FindLoader(AssetPath name);

  bool Load(int depth = -1);
#if defined(ORKCONFIG_ASSET_UNLOAD)
  bool unload(int depth = -1);
#endif

  AssetSetLevel* topLevel() const;

  void pushLevel(object::ObjectClass*);
  void popLevel();

private:
  AssetSetLevel* mTopLevel;
};

}} // namespace ork::asset
