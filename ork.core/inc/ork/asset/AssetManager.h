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
  //////////////////////////////////////////////////////////////////////////////
  static typed_asset_ptr_t load(
      const AssetPath& asset_name, //
      vars_constptr_t vmap = nullptr);
  //////////////////////////////////////////////////////////////////////////////
private:
  static ork::recursive_mutex gLock;
};

}} // namespace ork::asset
