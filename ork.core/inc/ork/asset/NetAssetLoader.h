////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/AssetLoader.h>
#include <ork/asset/catalog/asset_fetcher.h>
#include <ork/asset/catalog/asset_config.h>
#include <ork/asset/catalog/asset_manifest.h>

namespace ork::asset {

struct NetAssetLoader : public AssetLoader {
  
  NetAssetLoader();
  
  bool doesExist(const AssetPath&) override;
  bool resolvePath(
      const AssetPath& pathin, 
      AssetPath& resolved_path) override;
  asset_ptr_t load(loadrequest_ptr_t loadreq) override;
  void destroy(asset_ptr_t asset) override;
  std::set<file::Path> EnumerateExisting() override;
  
  void reloadManifests();
  
private:
  catalog::assetfetcher_ptr_t _fetcher;
  catalog::assetconfig_ptr_t _config;
  std::map<std::string, catalog::AssetManifest::AssetEntry> _manifest_cache;
  std::vector<file::Path> _manifest_dirs;
  
  // Cache management
  file::Path getCachePath(const std::string& asset_id) const;
  bool checkCache(const std::string& asset_id) const;
  asset_ptr_t loadFromCache(const file::Path& cache_path, loadrequest_ptr_t loadreq);
  
  // Asset fetching
  bool fetchAsset(const std::string& asset_id, loadrequest_ptr_t loadreq);
  void waitForDownload(const std::string& asset_id);
};

using netassetloader_ptr_t = std::shared_ptr<NetAssetLoader>;

} // namespace ork::asset