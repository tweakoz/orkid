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
#include <ork/asset/catalog/request.h>
#include <ork/kernel/datablock.h>
#include <ork/util/logger.h>

namespace ork::asset {

static logchannel_ptr_t logchan_hyb = logger()->configureChannel("HYBLOAD", fvec3(0.8, 0.5, 0.5), true);
///////////////////////////////////////////////////////////////////////////////

asset_ptr_t HybridAssetLoader::load(loadrequest_ptr_t loadreq) {
  auto path = loadreq->_asset_path;
  datablock_ptr_t dblock;
  
  if (path.isAssetCatalogPath()) {
    logchan_hyb->log("Loading from asset catalog: %s", path.c_str());
    // Load from asset catalog
    auto components = path.getCatalogComponents();
    if (components.isValid()) {
      auto catalog = asset::catalog::AssetCatalog::globalInstance();
      auto catalog_path = components._namespace + "|" + components._asset;
      auto result = catalog->fetch(catalog_path); // synchronous 
      //logchan_hyb->log("result %p", (void*) result.get());
      dblock = result ? result->_data : nullptr;
      //logchan_hyb->log("dblock %p", (void*) dblock.get());
    }
  } else if (path.isFilePath()) {
    logchan_hyb->log("Loading from filesystem: %s", path.c_str());
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