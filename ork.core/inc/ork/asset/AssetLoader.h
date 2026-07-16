////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/file/path.h>
#include <ork/kernel/mutex.h>
#include <set>

namespace ork::asset {

struct AssetLoader {

  using loader_by_ext_map_t = std::unordered_map<std::string, assetloader_ptr_t>;

  virtual ~AssetLoader() {}
  virtual bool doesExist(const AssetPath&) = 0;
  virtual bool resolvePath(
      const AssetPath& pathin, //
      AssetPath& resolved_path)                                    = 0;
  virtual asset_ptr_t load(loadrequest_ptr_t loadreq)              = 0;
  virtual void destroy(asset_ptr_t asset)                          = 0;

  virtual std::set<file::Path> EnumerateExisting() = 0;

  static void registerLoaderForExtension(std::string, assetloader_ptr_t);
  static LockedResource<loader_by_ext_map_t> _loaders_by_ext;

  // WS5: a loader that sets this declares load() safe to run concurrently
  // with other load() calls of the same type (its _doLoadAsset touches only
  // request-local state and thread-safe caches). AssetManager<T>::loadAsync
  // skips the per-type gLock for such loaders, letting decodes run in
  // parallel on the worker pool.
  bool _concurrent = false;
};

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset
