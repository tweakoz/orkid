////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/AssetLoader.h>
#include <ork/kernel/datablock.h>

namespace ork::asset {

////////////////////////////////////////////////////////////////////////////////
// HybridAssetLoader - Base class for loaders that support both
// file-based and catalog-based asset loading
//
// This class centralizes the logic for determining whether to load
// from the asset catalog or from the filesystem, eliminating duplication
// across different asset loaders.
////////////////////////////////////////////////////////////////////////////////

struct HybridAssetLoader : public AssetLoader {
  
  // Final load method - determines source and delegates to _doLoadFromDatablock
  asset_ptr_t load(loadrequest_ptr_t loadreq) override final;
  
  // Override these in base class if needed
  bool doesExist(const AssetPath& path) override;
  bool resolvePath(const AssetPath& in, AssetPath& out) override;
  void destroy(asset_ptr_t asset) override;
  std::set<file::Path> EnumerateExisting() override;
  
protected:
  // Subclasses must implement this to load from a datablock
  virtual asset_ptr_t _doLoadFromDatablock(
    loadrequest_ptr_t loadreq,
    datablock_ptr_t dblock) = 0;
};

} // namespace ork::asset