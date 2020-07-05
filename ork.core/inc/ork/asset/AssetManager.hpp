///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/pch.h>

#include <ork/object/ObjectClass.h>
#include <ork/asset/AssetLoader.h>
#include <ork/asset/AssetSet.h>
#include <ork/util/RingLink.hpp>
///////////////////////////////////////////////////////////////////////////////
namespace ork::asset {
///////////////////////////////////////////////////////////////////////////////
template <typename AssetType> ork::recursive_mutex AssetManager<AssetType>::gLock("AssetManagerMutex");
///////////////////////////////////////////////////////////////////////////////
template <typename AssetType> varmap::VarMap AssetManager<AssetType>::novars() {
  return varmap::VarMap();
};
///////////////////////////////////////////////////////////////////////////////
template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::load(
    const AssetPath& asset_name, //
    const varmap::VarMap& vmap) {
  auto loader = getLoader<AssetType>();
  gLock.Lock();
  auto asset = loader->load(asset_name);
  gLock.UnLock();
  return std::dynamic_pointer_cast<AssetType>(asset);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
