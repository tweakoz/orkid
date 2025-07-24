////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/asset/NetAssetLoader.h>
#include <ork/asset/Asset.h>
#include <ork/kernel/environment.h>
#include <ork/util/download_manager.h>
#include <ork/file/fileenv.h>
#include <filesystem>

namespace ork::asset {

///////////////////////////////////////////////////////////////////////////////

NetAssetLoader::NetAssetLoader() {
  // Initialize download manager if not provided
  auto dl_mgr = std::make_shared<DownloadManager>();
  _fetcher = std::make_shared<catalog::AssetFetcher>(dl_mgr);
  
  // Load manifest directories from environment
  std::string manifest_dirs_env;
  if (genviron.get("ORKID_ASSET_MANIFEST_DIRS", manifest_dirs_env)) {
    // Parse comma-separated directories
    size_t pos = 0;
    while (pos < manifest_dirs_env.length()) {
      size_t next = manifest_dirs_env.find(',', pos);
      if (next == std::string::npos) next = manifest_dirs_env.length();
      
      std::string dir = manifest_dirs_env.substr(pos, next - pos);
      if (!dir.empty()) {
        _manifest_dirs.push_back(file::Path(dir));
      }
      pos = next + 1;
    }
  }
  
  // Load manifests
  reloadManifests();
}

///////////////////////////////////////////////////////////////////////////////

void NetAssetLoader::reloadManifests() {
  if (!_manifest_dirs.empty()) {
    _fetcher->setManifestDirectories(_manifest_dirs);
  }
  _fetcher->reload();
  
  // Cache the resolved assets
  _manifest_cache = _fetcher->getAssets();
  _config = std::make_shared<catalog::AssetConfig>(_fetcher->getConfig());
}

///////////////////////////////////////////////////////////////////////////////

bool NetAssetLoader::doesExist(const AssetPath& path) {
  // For catalog assets, we check the manifest
  std::string asset_id = path.c_str();
  
  // Check if it exists in our manifest cache
  return _manifest_cache.find(asset_id) != _manifest_cache.end();
}

///////////////////////////////////////////////////////////////////////////////

bool NetAssetLoader::resolvePath(
    const AssetPath& pathin,
    AssetPath& resolved_path) {
  
  // For catalog assets, the path is the asset ID
  resolved_path = pathin;
  return doesExist(pathin);
}

///////////////////////////////////////////////////////////////////////////////

std::set<file::Path> NetAssetLoader::EnumerateExisting() {
  std::set<file::Path> result;
  
  // Return all asset IDs from the manifest
  for (const auto& [asset_id, entry] : _manifest_cache) {
    result.insert(file::Path(asset_id));
  }
  
  return result;
}

///////////////////////////////////////////////////////////////////////////////

file::Path NetAssetLoader::getCachePath(const std::string& asset_id) const {
  // Default cache location
  std::string cache_root;
  if (!genviron.get("ORKID_ASSET_CACHE_DIR", cache_root)) {
    cache_root = file::Path::temp_dir().c_str();
  }
  
  // Replace dots with slashes for hierarchical storage
  std::string path_part = asset_id;
  std::replace(path_part.begin(), path_part.end(), '.', '/');
  
  return file::Path(cache_root) / file::Path("catalog_cache") / file::Path(path_part);
}

///////////////////////////////////////////////////////////////////////////////

bool NetAssetLoader::checkCache(const std::string& asset_id) const {
  auto cache_path = getCachePath(asset_id);
  
  // Check if any file with the base name exists
  if (!FileEnv::DoesDirectoryExist(cache_path.getFolder(file::Path::EPATHTYPE_NATIVE))) {
    return false;
  }
  
  // Look for files with any extension
  auto parent_dir = cache_path.getFolder(file::Path::EPATHTYPE_NATIVE);
  auto base_name = cache_path.getName();
  
  // TODO: Implement directory listing to find matching files
  // For now, just check if the exact path exists
  return cache_path.doesPathExist();
}

///////////////////////////////////////////////////////////////////////////////

asset_ptr_t NetAssetLoader::loadFromCache(const file::Path& cache_path, loadrequest_ptr_t loadreq) {
  // Find the appropriate loader based on file extension
  std::string ext = cache_path.getExtension();
  
  assetloader_ptr_t file_loader;
  AssetLoader::_loaders_by_ext.atomicOp(
    [&ext, &file_loader](loader_by_ext_map_t& map) {
      auto it = map.find(ext);
      file_loader = (it != map.end()) ? it->second : nullptr;
    });
  
  if (!file_loader) {
    printf("No loader found for extension: %s\n", ext.c_str());
    return nullptr;
  }
  
  // Create a new load request with the cache path
  auto cache_loadreq = std::make_shared<LoadRequest>(cache_path, loadreq->_asset_vars);
  cache_loadreq->_on_load_complete = loadreq->_on_load_complete;
  cache_loadreq->_on_event = loadreq->_on_event;
  
  return file_loader->load(cache_loadreq);
}

///////////////////////////////////////////////////////////////////////////////

bool NetAssetLoader::fetchAsset(const std::string& asset_id, loadrequest_ptr_t loadreq) {
  // Set up progress callback if provided
  if (loadreq->_catalog_request && loadreq->_catalog_request->_progress_callback._item) {
    _fetcher->_on_asset_progress._item = 
      [loadreq, asset_id](const std::string& id, size_t down, size_t total) {
        if (id == asset_id && loadreq->_catalog_request->_progress_callback._item) {
          loadreq->_catalog_request->_progress_callback._item(down, total);
        }
      };
  }
  
  // Fetch the asset
  int fetched = _fetcher->fetchPak(asset_id);
  return fetched > 0;
}

///////////////////////////////////////////////////////////////////////////////

void NetAssetLoader::waitForDownload(const std::string& asset_id) {
  // Simple polling wait - in production, this should use proper synchronization
  const int max_wait_ms = 30000; // 30 seconds default
  const int poll_interval_ms = 100;
  
  int waited = 0;
  while (waited < max_wait_ms) {
    if (checkCache(asset_id)) {
      return;
    }
    
    usleep(poll_interval_ms * 1000);
    waited += poll_interval_ms;
  }
  
  printf("Timeout waiting for asset download: %s\n", asset_id.c_str());
}

///////////////////////////////////////////////////////////////////////////////

asset_ptr_t NetAssetLoader::load(loadrequest_ptr_t loadreq) {
  if (!loadreq->isCatalogLoad()) {
    printf("NetAssetLoader: LoadRequest is not a catalog load\n");
    return nullptr;
  }
  
  // Get the asset identifier
  std::string asset_id = loadreq->getAssetIdentifier();
  
  // Check if asset exists in manifest
  auto it = _manifest_cache.find(asset_id);
  if (it == _manifest_cache.end()) {
    printf("Asset not found in catalog: %s\n", asset_id.c_str());
    return nullptr;
  }
  
  const auto& asset_entry = it->second;
  
  // Determine cache path with proper extension
  auto cache_base = getCachePath(asset_id);
  auto cache_path = file::Path(cache_base.c_str() + asset_entry._filename);
  
  // Check cache first
  if (cache_path.doesPathExist()) {
    // Verify MD5 if requested
    if (!asset_entry._md5.empty()) {
      // TODO: Implement MD5 verification
    }
    
    return loadFromCache(cache_path, loadreq);
  }
  
  // Need to fetch from network
  loadreq->incrementPartialLoadCount();
  
  if (loadreq->_on_load_complete) {
    // Async path
    _fetcher->_on_asset_complete._item = 
      [this, loadreq, asset_id, cache_path](const std::string& id, bool success) {
        if (id == asset_id && success) {
          auto asset = loadFromCache(cache_path, loadreq);
          if (asset) {
            loadreq->_asset = asset;
            loadreq->decrementPartialLoadCount();
          }
        }
      };
    
    fetchAsset(asset_id, loadreq);
    return nullptr; // Will complete asynchronously
    
  } else {
    // Sync path
    if (fetchAsset(asset_id, loadreq)) {
      waitForDownload(asset_id);
      auto asset = loadFromCache(cache_path, loadreq);
      loadreq->decrementPartialLoadCount();
      return asset;
    }
  }
  
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void NetAssetLoader::destroy(asset_ptr_t asset) {
  // Nothing special to do for catalog assets
  // The underlying file loader will handle cleanup
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset