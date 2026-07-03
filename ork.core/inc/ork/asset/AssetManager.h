////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/kernel/mutex.h>
#include <ork/file/path.h>

namespace ork { namespace asset {

struct FileAssetLoader;

///////////////////////////////////////////////////////////////////////////////

template <typename AssetType> struct AssetManager {

  using typed_asset_ptr_t      = std::shared_ptr<AssetType>;
  using typed_asset_constptr_t = std::shared_ptr<const AssetType>;

  static typed_asset_ptr_t load(loadrequest_ptr_t lreq); // async load with options
  static typed_asset_ptr_t load(const AssetPath& pth); // default async load

  // WS5: enqueue the load on the worker pool and return immediately.
  // The request's partial-load counter is held from here until the worker
  // finishes, so a lev2::LoadJoinSet::adopt(lreq) + join() covers the whole
  // load; the asset lands in lreq->_asset (cast via assetAs after join).
  // Loaders marked _concurrent run WITHOUT the per-type gLock (parallel
  // decodes); others serialize exactly as the sync path does.
  static void loadAsync(loadrequest_ptr_t lreq);
  static typed_asset_ptr_t assetAs(loadrequest_ptr_t lreq);

private:
  static ork::recursive_mutex gLock;
};

}} // namespace ork::asset
