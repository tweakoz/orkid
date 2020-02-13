///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/pch.h>

#include <ork/asset/AssetLoader.h>
#include <ork/util/RingLink.hpp>

namespace ork { namespace asset {

template <typename AssetType> ork::recursive_mutex AssetManager<AssetType>::gLock("AssetManagerMutex");

template <typename AssetType> VarMap AssetManager<AssetType>::_gnovars;

template <typename AssetType> bool AssetManager<AssetType>::gbAUTOLOAD = true;

template <typename AssetType> inline AssetType* AssetManager<AssetType>::Create(const PieceString& asset_name, const VarMap& vmap) {
  gLock.Lock();
  AssetType* prval = rtti::safe_downcast<AssetType*>(AssetType::GetClassStatic()->DeclareAsset(asset_name, vmap));
  gLock.UnLock();
  return prval;
}

template <typename AssetType> inline AssetType* AssetManager<AssetType>::Find(const PieceString& asset_name) {
  gLock.Lock();
  AssetType* prval = rtti::safe_downcast<AssetType*>(AssetType::GetClassStatic()->FindAsset(asset_name));
  gLock.UnLock();
  return prval;
}

template <typename AssetType> inline AssetType* AssetManager<AssetType>::Load(const PieceString& asset_name) {
  gLock.Lock();

  AssetType* asset = Create(asset_name);

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
template <typename AssetType> inline AssetType* AssetManager<AssetType>::LoadUnManaged(const PieceString& asset_name) {
  gLock.Lock();
  AssetType* prval = rtti::safe_downcast<AssetType*>(AssetType::GetClassStatic()->LoadUnManagedAsset(asset_name));
  gLock.UnLock();
  return prval;
}

template <typename AssetType> inline bool AssetManager<AssetType>::AutoLoad(int depth) {
  if (gbAUTOLOAD) {
    gLock.Lock();
    bool brval = AssetType::GetClassStatic()->GetAssetSet().Load(depth);
    gLock.UnLock();
    return brval;
  } else {
    return false;
  }
}

#if defined(ORKCONFIG_ASSET_UNLOAD)
template <typename AssetType> inline bool AssetManager<AssetType>::AutoUnLoad(int depth) {
  return AssetType::GetClassStatic()->GetAssetSet().UnLoad(depth);
}
#endif

}} // namespace ork::asset
