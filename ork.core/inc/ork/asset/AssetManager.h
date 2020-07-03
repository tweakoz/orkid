///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/kernel/mutex.h>
#include <ork/file/path.h>

namespace ork { namespace asset {

class FileAssetLoader;

template <typename AssetType> class AssetManager {

  using typed_asset_ptr_t      = std::shared_ptr<AssetType>;
  using typed_asset_constptr_t = std::shared_ptr<const AssetType>;

public:
  static varmap::VarMap novars();
  static typed_asset_ptr_t Create(const AssetPath& asset_name, const varmap::VarMap& vmap = novars());
  static typed_asset_ptr_t Find(const AssetPath& asset_name);
  static typed_asset_ptr_t Load(const AssetPath& asset_name);
  static typed_asset_ptr_t LoadUnManaged(const AssetPath& asset_name);
  static bool AutoLoad(int depth = -1);
#if defined(ORKCONFIG_ASSET_UNLOAD)
  static bool AutoUnLoad(int depth = -1);
#endif

  static void DisableAutoLoad() {
    gbAUTOLOAD = false;
  }
  static void EnableAutoLoad() {
    gbAUTOLOAD = true;
  }

private:
  static ork::recursive_mutex gLock;
  static bool gbAUTOLOAD;
};

}} // namespace ork::asset
