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

namespace ork { namespace asset {

template <typename AssetType> ork::recursive_mutex AssetManager<AssetType>::gLock("AssetManagerMutex");

template <typename AssetType> varmap::VarMap AssetManager<AssetType>::novars() {
  return varmap::VarMap();
};

template <typename AssetType> bool AssetManager<AssetType>::gbAUTOLOAD = true;

template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::Create(const AssetPath& asset_name, const varmap::VarMap& vmap) {
  gLock.Lock();
  OrkAssert(false);
  // auto asset       = AssetType::GetClassStatic()->DeclareAsset(asset_name, vmap);
  // auto typed_asset = std::dynamic_pointer_cast<AssetType>(asset);
  gLock.UnLock();
  return typename AssetManager<AssetType>::typed_asset_ptr_t(nullptr);
}

template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::Find(const AssetPath& asset_name) {
  auto clazz    = AssetType::GetClassStatic();
  auto objclazz = dynamic_cast<object::ObjectClass*>(clazz);
  gLock.Lock();
  OrkAssert(false);
  // auto asset       = AssetType::GetClassStatic()->FindAsset(asset_name);
  // auto typed_asset = std::dynamic_pointer_cast<AssetType>(asset);
  gLock.UnLock();
  // return typed_asset;
  return typename AssetManager<AssetType>::typed_asset_ptr_t(nullptr);
}

template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::Load(const AssetPath& asset_name) {
  gLock.Lock();
  OrkAssert(false);

  typed_asset_ptr_t asset = Create(asset_name);

  if (asset) {
    if (false == asset->IsLoaded()) {
      asset->Load();
    }
    gLock.UnLock();
    return asset;
  }
  gLock.UnLock();
  return NULL;
}
template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::LoadUnManaged(const AssetPath& asset_name) {
  gLock.Lock();
  // auto asset       = AssetType::GetClassStatic()->LoadUnManagedAsset(asset_name);
  // auto typed_asset = std::dynamic_pointer_cast<AssetType>(asset);
  gLock.UnLock();
  return typename AssetManager<AssetType>::typed_asset_ptr_t(nullptr);
  // return typed_asset;
}

template <typename AssetType> inline bool AssetManager<AssetType>::AutoLoad(int depth) {
  if (gbAUTOLOAD) {
    gLock.Lock();
    // auto asset_set = AssetType::GetClassStatic()->assetSet();
    // bool brval     = asset_set->Load(depth);
    gLock.UnLock();
    return false;
  } else {
    return false;
  }
}

#if defined(ORKCONFIG_ASSET_UNLOAD)
template <typename AssetType> inline bool AssetManager<AssetType>::AutoUnLoad(int depth) {
  // auto asset_set = AssetType::GetClassStatic()->assetSet();
  // return asset_set->UnLoad(depth);
}
#endif

}} // namespace ork::asset
