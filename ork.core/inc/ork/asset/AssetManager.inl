////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>

#include <ork/object/ObjectClass.h>
#include <ork/asset/AssetLoader.h>
#include <ork/asset/AssetSet.h>
#include <ork/util/RingLink.hpp>
#include <ork/asset/AssetManager.h>
#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::asset {
///////////////////////////////////////////////////////////////////////////////
template <typename AssetType> ork::recursive_mutex AssetManager<AssetType>::gLock("AssetManagerMutex");
///////////////////////////////////////////////////////////////////////////////
template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::load(loadrequest_ptr_t loadreq) {
  auto loader = getLoader<AssetType>();
  gLock.Lock();
  auto asset = loader->load(loadreq);
  gLock.UnLock();
  return std::dynamic_pointer_cast<AssetType>(asset);
}
///////////////////////////////////////////////////////////////////////////////
template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::load(const AssetPath& pth) {
  auto loader = getLoader<AssetType>();
  gLock.Lock();
  auto loadreq = std::make_shared<LoadRequest>(pth);
  auto asset = loader->load(loadreq);
  gLock.UnLock();
  return std::dynamic_pointer_cast<AssetType>(asset);
}
///////////////////////////////////////////////////////////////////////////////
// WS5: worker-pool load. The ticket is taken HERE (not in the worker) so a
// joiner that adopts the request immediately after this call can never see
// a zero counter while the load is still queued. loader->load() sets
// lreq->_asset before FileAssetLoader's own inner ticket releases, so by the
// time THIS ticket releases the asset pointer is published.
template <typename AssetType>
inline void AssetManager<AssetType>::loadAsync(loadrequest_ptr_t lreq) {
  lreq->incrementPartialLoadCount();
  opq::concurrentQueue()->enqueue([lreq]() {
    auto loader = getLoader<AssetType>();
    if (loader->_concurrent) {
      loader->load(lreq);
    } else {
      gLock.Lock();
      loader->load(lreq);
      gLock.UnLock();
    }
    lreq->decrementPartialLoadCount();
  });
}
///////////////////////////////////////////////////////////////////////////////
template <typename AssetType>
inline typename AssetManager<AssetType>::typed_asset_ptr_t //
AssetManager<AssetType>::assetAs(loadrequest_ptr_t lreq) {
  return std::dynamic_pointer_cast<AssetType>(lreq->_asset);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
