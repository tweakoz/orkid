////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/kernel/svariant.h>
#include <ork/util/download_manager.h>
#include <ork/asset/catalog/asset_manifest.h>
#include <ork/asset/catalog/asset_config.h>
#include <functional>
#include <memory>

namespace ork::asset::catalog {

struct AssetFetcher;
using assetfetcher_ptr_t = std::shared_ptr<AssetFetcher>;

////////////////////////////////////////////////////////////////////////////////

struct AssetFetcher {
  
  //////////////////////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////////////////////
  
  // Progress callback: (asset_id, downloaded_bytes, total_bytes)
  using progress_fn_t = std::function<void(const std::string&, size_t, size_t)>;
  ItemAndData<progress_fn_t> _on_asset_progress;
  
  // Completion callback: (asset_id, success)
  using complete_fn_t = std::function<void(const std::string&, bool)>;
  ItemAndData<complete_fn_t> _on_asset_complete;
  
  //////////////////////////////////////////////////////////////////////////////
  // Constructor
  //////////////////////////////////////////////////////////////////////////////
  
  explicit AssetFetcher(downloadmanager_ptr_t download_mgr = nullptr);
  ~AssetFetcher();
  
  //////////////////////////////////////////////////////////////////////////////
  // Main API
  //////////////////////////////////////////////////////////////////////////////
  
  // Fetch by full ID (namespace.asset_id) or namespace only
  // Returns number of assets fetched
  int fetchPak(const std::string& pack_identifier);
  
  //////////////////////////////////////////////////////////////////////////////
  // Configuration
  //////////////////////////////////////////////////////////////////////////////
  
  // Set manifest directories (default from ORKID_ASSET_MANIFEST_DIRS env)
  void setManifestDirectories(const std::vector<file::Path>& dirs);
  
  // Get loaded manifests and config
  const std::map<std::string, AssetManifest::AssetEntry>& getAssets() const { return _resolved_assets; }
  const AssetConfig& getConfig() const { return _config; }
  
  // Reload manifests and configs
  void reload();
  
private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
  
  downloadmanager_ptr_t _download_manager;
  AssetConfig _config;
  std::map<std::string, AssetManifest::AssetEntry> _resolved_assets;
  std::vector<file::Path> _manifest_dirs;
  
  //////////////////////////////////////////////////////////////////////////////
  // Internal methods
  //////////////////////////////////////////////////////////////////////////////
  
  // Load all manifests with priority resolution
  void loadAllManifests();
  
  // Load and merge all config files
  void loadAllConfigs();
  
  // Queue asset fetch (non-blocking)
  void queueAssetFetch(const std::string& asset_id, 
                       const AssetManifest::AssetEntry& asset_data,
                       std::function<void()> on_complete);
                       
  // Process downloaded assets
  bool processAssetPak(const std::string& asset_id,
                       const AssetManifest::AssetEntry& asset_data,
                       const file::Path& temp_file,
                       const file::Path& dest_path);
                       
  bool processAsset(const std::string& asset_id,
                    const AssetManifest::AssetEntry& asset_data,
                    const file::Path& temp_file,
                    const file::Path& dest_path);
                    
  // Process assets from cache (don't delete cached files)
  bool processAssetPakFromCache(const std::string& asset_id,
                                const AssetManifest::AssetEntry& asset_data,
                                const file::Path& cache_file,
                                const file::Path& dest_path);
                                
  bool processAssetFromCache(const std::string& asset_id,
                             const AssetManifest::AssetEntry& asset_data,
                             const file::Path& cache_file,
                             const file::Path& dest_path);
                  
  // Subprocess operations using Spawner
  bool decryptFile(const file::Path& src, const file::Path& dst, 
                   const std::string& key);
  bool extractTar(const file::Path& tar_file, const file::Path& dest_dir);
  
  // MD5 verification
  bool verifyMD5(const file::Path& file, const std::string& expected_md5);
  
  // Get default manifest directories from environment
  std::vector<file::Path> getDefaultManifestDirs() const;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog