////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/asset/HybridAssetLoader.h>
#include <ork/file/path.h>
#include <ork/file/file.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/kernel/datablock.h>

namespace ork::asset {

///////////////////////////////////////////////////////////////////////////////

asset_ptr_t HybridAssetLoader::load(loadrequest_ptr_t loadreq) {
  auto path = loadreq->_asset_path;
  datablock_ptr_t dblock;
  
  if (path.isAssetCatalogPath()) {
    // Load from asset catalog
    auto components = path.getCatalogComponents();
    if (components.isValid()) {
      auto catalog = asset::catalog::AssetCatalog::globalInstance();
      auto catalog_path = components._namespace + "|" + components._asset;
      auto result = catalog->get(catalog_path);
      dblock = result ? result->_data : nullptr;
    }
  } else if (path.isFilePath()) {
    // Load from filesystem
    auto abs_path = path.toAbsolute();
    if (abs_path.doesPathExist()) {
      dblock = File::loadDatablock(abs_path);
    }
  }
  
  if (dblock) {
    return _doLoadFromDatablock(loadreq, dblock);
  }
  
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

bool HybridAssetLoader::doesExist(const AssetPath& path) {
  if (path.isAssetCatalogPath()) {
    auto components = path.getCatalogComponents();
    if (components.isValid()) {
      auto catalog = asset::catalog::AssetCatalog::globalInstance();
      auto fq_asset_id = components._namespace + "|" + components._asset;
      return catalog->hasAsset(fq_asset_id);
    }
    return false;
  }
  return path.doesPathExist();
}

///////////////////////////////////////////////////////////////////////////////

bool HybridAssetLoader::resolvePath(const AssetPath& in, AssetPath& out) {
  out = in;
  return doesExist(in);
}

///////////////////////////////////////////////////////////////////////////////

void HybridAssetLoader::destroy(asset_ptr_t asset) {
  // Default implementation - subclasses can override if needed
}

///////////////////////////////////////////////////////////////////////////////

std::set<file::Path> HybridAssetLoader::EnumerateExisting() {
  // Default implementation - subclasses can override if needed
  return std::set<file::Path>();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset