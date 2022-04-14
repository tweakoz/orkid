////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/kernel/mutex.h>
#include <ork/file/path.h>

namespace ork { namespace asset {

struct FileAssetLoader;

template <typename AssetType> struct AssetManager {

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
