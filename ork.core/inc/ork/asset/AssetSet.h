////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/asset/Asset.h>

namespace ork { namespace asset {

struct AssetEntry;
struct AssetLoader;
struct AssetSetLevel;
struct AssetSetEntry;

struct AssetSet {
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
