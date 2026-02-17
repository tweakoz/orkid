////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/AssetManager.h>
#include <unordered_map>
#include <string>
#include <memory>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

/// Generic per-scope asset cache template.
///
/// Caches loaded assets by path string so that repeated loads of the
/// same asset return the cached instance instead of hitting disk.
///
/// Designed to live on a system (e.g. SceneGraphSystem) or standalone.
/// The cache is destroyed with its owner, so assets are naturally
/// released on simulation rebuild.
///
/// Cache key is currently the asset path string. When asset vars
/// (load modifiers) are used, the key should be extended to include
/// a hash of the vars contents — see TODO below.

template <typename AssetType>
struct AssetCache {

  using typed_asset_ptr_t = std::shared_ptr<AssetType>;

  /// Load asset from cache or disk. First call for a given path
  /// loads via AssetManager; subsequent calls return the cached result.
  typed_asset_ptr_t fetch(asset::loadrequest_ptr_t loadreq) {
    auto key = _makeKey(loadreq);
    auto it = _cache.find(key);
    if (it != _cache.end()) {
      return it->second;
    }
    auto asset = asset::AssetManager<AssetType>::load(loadreq);
    if (asset) {
      _cache[key] = asset;
    }
    return asset;
  }

  /// Convenience: fetch by path (creates a default LoadRequest).
  typed_asset_ptr_t fetch(const AssetPath& path) {
    auto loadreq = std::make_shared<asset::LoadRequest>(path);
    return fetch(loadreq);
  }

  /// Check if an asset is already cached for the given path.
  bool contains(const AssetPath& path) const {
    return _cache.find(path.c_str()) != _cache.end();
  }

  /// Clear all cached assets.
  void clear() {
    _cache.clear();
  }

  size_t size() const {
    return _cache.size();
  }

private:

  // TODO: when asset vars are used, extend the key to include
  // a hash of loadreq->_asset_vars contents so that the same path
  // loaded with different vars produces separate cache entries.
  static std::string _makeKey(asset::loadrequest_ptr_t loadreq) {
    return loadreq->_asset_path.c_str();
  }

  std::unordered_map<std::string, typed_asset_ptr_t> _cache;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
