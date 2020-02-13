///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/kernel/mutex.h>
#include <ork/file/path.h>

namespace ork {
class PieceString;
};

namespace ork { namespace asset {

class FileAssetLoader;

template <typename AssetType> class AssetManager {
public:
  static VarMap _gnovars;
  static AssetType* Create(const PieceString& asset_name, const VarMap& vmap = _gnovars);
  static AssetType* Find(const PieceString& asset_name);
  static AssetType* Load(const PieceString& asset_name);
  static AssetType* LoadUnManaged(const PieceString& asset_name);
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
