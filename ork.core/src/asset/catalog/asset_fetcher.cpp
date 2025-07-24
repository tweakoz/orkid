////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/asset_fetcher.h>
#include <ork/kernel/spawner.h>
#include <ork/kernel/environment.h>
#include <ork/file/file.h>
#include <ork/file/fileenv.h>
#include <ork/application/application.h>
#include <ork/util/crc.h>
#include <ork/kernel/string/string.h>
#include <glob.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <mutex>
#include <condition_variable>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////////////////////

struct AssetFetcher::Impl {
  std::mutex _mutex;
};

////////////////////////////////////////////////////////////////////////////////

AssetFetcher::AssetFetcher(downloadmanager_ptr_t download_mgr) 
  : _impl(std::make_unique<Impl>())
  , _download_manager(download_mgr ? download_mgr : std::make_shared<DownloadManager>()) {
  
  // Load default manifest directories
  _manifest_dirs = getDefaultManifestDirs();
  
  // Initial load
  reload();
}

////////////////////////////////////////////////////////////////////////////////

AssetFetcher::~AssetFetcher() {
}

////////////////////////////////////////////////////////////////////////////////

void AssetFetcher::setManifestDirectories(const std::vector<file::Path>& dirs) {
  _manifest_dirs = dirs;
  reload();
}

////////////////////////////////////////////////////////////////////////////////

void AssetFetcher::reload() {
  loadAllConfigs();
  loadAllManifests();
}

////////////////////////////////////////////////////////////////////////////////

std::vector<file::Path> AssetFetcher::getDefaultManifestDirs() const {
  std::vector<file::Path> dirs;
  
  // Get from environment
  std::string env_dirs;
  if (genviron.get("ORKID_ASSET_MANIFEST_DIRS", env_dirs)) {
    // Split by colon
    size_t start = 0;
    size_t end = env_dirs.find(':');
    while (end != std::string::npos) {
      dirs.push_back(file::Path(env_dirs.substr(start, end - start)));
      start = end + 1;
      end = env_dirs.find(':', start);
    }
    dirs.push_back(file::Path(env_dirs.substr(start)));
  } else {
    // Default location
    file::Path orkid_root = file::Path::orkroot_dir();
    dirs.push_back(orkid_root / "ork.data" / "asset_manifests");
  }
  
  return dirs;
}

////////////////////////////////////////////////////////////////////////////////

void AssetFetcher::loadAllConfigs() {
  _config = AssetConfig();  // Start fresh
  
  // Load and merge configs from each directory
  for (const auto& dir : _manifest_dirs) {
    if (!FileEnv::DoesDirectoryExist(dir)) {
      continue;
    }
    
    auto dir_config = AssetConfig::loadFromDirectory(dir);
    if (dir_config) {
      _config.merge(*dir_config);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void AssetFetcher::loadAllManifests() {
  _resolved_assets.clear();
  
  // Process each manifest directory
  for (const auto& manifest_dir : _manifest_dirs) {
    //printf("Checking manifest dir: %s\n", manifest_dir.c_str());
    if (!FileEnv::DoesDirectoryExist(manifest_dir)) {
      //printf("  Directory does not exist\n");
      continue;
    }
    
    // Find all JSON files in directory
    std::string pattern = manifest_dir.c_str();
    pattern += "/*.json";
    //printf("  Glob pattern: %s\n", pattern.c_str());
    
    glob_t glob_result;
    int glob_ret = glob(pattern.c_str(), GLOB_TILDE, nullptr, &glob_result);
    if (glob_ret == 0) {
      //printf("  Found %zu files\n", glob_result.gl_pathc);
      for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
        file::Path manifest_file(glob_result.gl_pathv[i]);
        
        // Skip config files
        std::string filename = manifest_file.toAbsolute().c_str();
        if (filename.find("config.json") != std::string::npos) {
          continue;
        }
        
        //printf("    Processing file: %s\n", manifest_file.c_str());
        
        auto manifest = AssetManifest::loadFromFile(manifest_file);
        if (manifest) {
          //printf("    Loaded manifest with %zu assets\n", manifest->_assets.size());
          // Add all assets with priority resolution
          for (const auto& [asset_id, asset_data] : manifest->_assets) {
            std::string full_id = manifest->_namespace + "." + asset_id;
            
            // Priority resolution
            auto it = _resolved_assets.find(full_id);
            if (it != _resolved_assets.end()) {
              if (asset_data._priority < it->second._priority) {
                _resolved_assets[full_id] = asset_data;
              }
            } else {
              _resolved_assets[full_id] = asset_data;
            }
          }
        }
      }
      globfree(&glob_result);
    } else {
      //printf("  Glob failed with error %d\n", glob_ret);
    }
  }
  //printf("Total resolved assets: %zu\n", _resolved_assets.size());
}

////////////////////////////////////////////////////////////////////////////////

int AssetFetcher::fetchPak(const std::string& pack_identifier) {
  int fetch_count = 0;
  
  if (pack_identifier.find('.') != std::string::npos) {
    ///////////////////////////////////////////////////////////
    // Full format: namespace.asset_id
    ///////////////////////////////////////////////////////////
    auto it = _resolved_assets.find(pack_identifier);
    if (it != _resolved_assets.end()) {
      if (fetchAsset(pack_identifier, it->second)) {
        fetch_count = 1;
      }
    } else {
      printf("Asset %s not found in manifests\n", pack_identifier.c_str());
    }
  } else {
    ///////////////////////////////////////////////////////////
    // Namespace only - fetch all assets in that namespace
    ///////////////////////////////////////////////////////////
    for (const auto& [asset_id, asset_data] : _resolved_assets) {
      size_t dot_pos = asset_id.find('.');
      if (dot_pos != std::string::npos) {
        std::string namespace_part = asset_id.substr(0, dot_pos);
        if (namespace_part == pack_identifier) {
          if (fetchAsset(asset_id, asset_data)) {
            fetch_count++;
          }
        }
      }
    }
    
    if (fetch_count == 0) {
      printf("No assets found matching '%s'\n", pack_identifier.c_str());
      printf("Note: You must specify the full asset ID as namespace.asset_id (e.g., singularity.std)\n");
      printf("Or you can specify just a namespace to fetch all assets in that namespace\n");
    }
  }
  
  return fetch_count;
}

////////////////////////////////////////////////////////////////////////////////

bool AssetFetcher::fetchAsset(const std::string& asset_id, const AssetManifest::AssetEntry& asset_data) {
  printf("Fetching asset %s\n", asset_id.c_str());
  
  ///////////////////////////////////////////////////////////
  // Resolve source and destination paths
  ///////////////////////////////////////////////////////////
  std::string src_loc = asset_data._src_loc;
  std::string dst_loc = asset_data._dst_loc;
  
  // Resolve source URL
  URL source_url;
  if (src_loc.find("<") == 0) {
    source_url = _config.resolveURL(src_loc);
    if (!asset_data._filename.empty()) {
      source_url = source_url / asset_data._filename;
    }
  } else {
    source_url = URL(src_loc);
  }
  
  // Resolve destination path
  file::Path dest_path = _config.resolvePath(dst_loc);
  
  // Create download
  file::Path temp_file = file::Path::temp_dir() / asset_data._filename;
  auto download = _download_manager->download(source_url, temp_file);
  
  ///////////////////////////////////////////////////////////
  // Set up download callbacks
  ///////////////////////////////////////////////////////////
  download->_on_progress._item = [this, asset_id](size_t downloaded, size_t total) {
    if (_on_asset_progress._item) {
      _on_asset_progress._item(asset_id, downloaded, total);
    }
  };
  
  bool download_success = false;
  std::mutex completion_mutex;
  std::condition_variable completion_cv;
  
  download->_on_complete._item = [&](bool success, const file::Path& path) {
    std::lock_guard<std::mutex> lock(completion_mutex);
    download_success = success;
    completion_cv.notify_one();
  };
  
  download->_on_failure._item = [&](const std::string& error) {
    printf("Download failed for %s: %s\n", asset_id.c_str(), error.c_str());
    std::lock_guard<std::mutex> lock(completion_mutex);
    download_success = false;
    completion_cv.notify_one();
  };
  
  // Wait for download completion
  {
    std::unique_lock<std::mutex> lock(completion_mutex);
    completion_cv.wait(lock);
  }
  
  if (!download_success) {
    if (_on_asset_complete._item) {
      _on_asset_complete._item(asset_id, false);
    }
    return false;
  }
  
  ///////////////////////////////////////////////////////////
  // Verify MD5
  ///////////////////////////////////////////////////////////
  if (!asset_data._md5.empty()) {
    if (!verifyMD5(temp_file, asset_data._md5)) {
      printf("MD5 verification failed for %s\n", asset_id.c_str());
      if (_on_asset_complete._item) {
        _on_asset_complete._item(asset_id, false);
      }
      return false;
    }
  }
  
  ///////////////////////////////////////////////////////////
  // Process based on asset type
  ///////////////////////////////////////////////////////////
  bool process_success = false;
  
  if (asset_data._type == "asset_pak") {
    // Decrypt if needed
    file::Path decrypted_file = temp_file;
    auto ns_it = _config._namespace_keys.find(asset_data._namespace);
    if (ns_it != _config._namespace_keys.end()) {
      decrypted_file = file::Path(temp_file.c_str() + std::string(".dec"));
      printf("Decrypting asset_pak %s\n", asset_id.c_str());
      if (!decryptFile(temp_file, decrypted_file, ns_it->second)) {
        printf("Decryption failed for %s\n", asset_id.c_str());
        if (_on_asset_complete._item) {
          _on_asset_complete._item(asset_id, false);
        }
        return false;
      }
    }
    
    // Extract tar
    printf("Extracting asset_pak %s\n", asset_id.c_str());
    process_success = extractTar(decrypted_file, dest_path);
    
    // Clean up temporary files
    std::remove(decrypted_file.c_str());
    if (decrypted_file != temp_file) {
      std::remove(temp_file.c_str());
    }
    
  } else if (asset_data._type == "asset") {
    // Single file asset
    file::Path final_dest = dest_path;
    
    // Decrypt if needed
    auto ns_it = _config._namespace_keys.find(asset_data._namespace);
    if (ns_it != _config._namespace_keys.end()) {
      printf("Decrypting asset %s\n", asset_id.c_str());
      process_success = decryptFile(temp_file, final_dest, ns_it->second);
      std::remove(temp_file.c_str());
    } else {
      // Just move the file
      // Copy file
      std::ifstream src(temp_file.c_str(), std::ios::binary);
      std::ofstream dst(final_dest.c_str(), std::ios::binary);
      if (src && dst) {
        dst << src.rdbuf();
        process_success = true;
      } else {
        process_success = false;
      }
      std::remove(temp_file.c_str());
    }
  }
  
  if (_on_asset_complete._item) {
    _on_asset_complete._item(asset_id, process_success);
  }
  
  return process_success;
}

////////////////////////////////////////////////////////////////////////////////

bool AssetFetcher::verifyMD5(const file::Path& file, const std::string& expected_md5) {
  // Read file and calculate MD5
  std::ifstream ifs(file.c_str(), std::ios::binary);
  if (!ifs.is_open()) {
    return false;
  }
  
  // TODO: Implement proper MD5 calculation
  // For now, just skip verification
  ifs.close();
  
  printf("Warning: MD5 verification not yet implemented, skipping check\n");
  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool AssetFetcher::decryptFile(const file::Path& src, const file::Path& dst, const std::string& key) {
  // Use GPG for decryption (matching Python implementation)
  Spawner spawner;
  spawner.mWorkingDirectory = file::Path::temp_dir().c_str();
  
  // Build command line
  std::string cmd = "gpg --quiet --decrypt --passphrase ";
  cmd += key;
  cmd += " --batch --output ";
  cmd += std::string(dst.c_str());
  cmd += " ";
  cmd += std::string(src.c_str());
  
  spawner.mCommandLine = cmd;
  spawner.spawnSynchronous();
  
  return spawner.mExecRet == 0;
}

////////////////////////////////////////////////////////////////////////////////

bool AssetFetcher::extractTar(const file::Path& tar_file, const file::Path& dest_dir) {
  // Ensure destination directory exists
  std::string mkdir_cmd = "mkdir -p ";
  mkdir_cmd += dest_dir.c_str();
  system(mkdir_cmd.c_str());
  
  // Use tar for extraction
  Spawner spawner;
  // Don't set working directory - use -C flag instead
  
  // Build command line matching Python implementation
  std::string cmd = "tar xf ";
  cmd += tar_file.c_str();
  cmd += " -C ";
  cmd += dest_dir.c_str();
  
  spawner.mCommandLine = cmd;
  spawner.spawnSynchronous();
  
  return spawner.mExecRet == 0;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog