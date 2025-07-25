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
#include <ork/util/md5.h>
#include <ork/kernel/string/string.h>
#include <glob.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sys/stat.h>

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
  std::vector<std::pair<std::string, AssetManifest::AssetEntry>> assets_to_fetch;
  
  if (pack_identifier.find('.') != std::string::npos) {
    ///////////////////////////////////////////////////////////
    // Full format: namespace.asset_id
    ///////////////////////////////////////////////////////////
    auto it = _resolved_assets.find(pack_identifier);
    if (it != _resolved_assets.end()) {
      assets_to_fetch.push_back({pack_identifier, it->second});
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
          assets_to_fetch.push_back({asset_id, asset_data});
        }
      }
    }
    
    if (assets_to_fetch.empty()) {
      printf("No assets found matching '%s'\n", pack_identifier.c_str());
      printf("Note: You must specify the full asset ID as namespace.asset_id (e.g., singularity.std)\n");
      printf("Or you can specify just a namespace to fetch all assets in that namespace\n");
      return 0;
    }
  }
  
  ///////////////////////////////////////////////////////////
  // Queue all downloads and processing in parallel
  ///////////////////////////////////////////////////////////
  std::atomic<int> completed_count{0};
  std::mutex completion_mutex;
  std::condition_variable completion_cv;
  const int total_assets = assets_to_fetch.size();
  
  for (const auto& [asset_id, asset_data] : assets_to_fetch) {
    // Queue the download and post-processing
    queueAssetFetch(asset_id, asset_data, 
      [&completed_count, &completion_mutex, &completion_cv, total_assets]() {
        int count = ++completed_count;
        if (count == total_assets) {
          std::lock_guard<std::mutex> lock(completion_mutex);
          completion_cv.notify_one();
        }
      });
  }
  
  // Wait for all assets to complete
  {
    std::unique_lock<std::mutex> lock(completion_mutex);
    completion_cv.wait(lock, [&completed_count, total_assets]() {
      return completed_count >= total_assets;
    });
  }
  
  return completed_count.load();
}

////////////////////////////////////////////////////////////////////////////////

void AssetFetcher::queueAssetFetch(const std::string& asset_id, 
                                   const AssetManifest::AssetEntry& asset_data,
                                   std::function<void()> on_complete) {
  printf("Queueing download for asset %s\n", asset_id.c_str());
  
  ///////////////////////////////////////////////////////////
  // Resolve source and destination paths
  ///////////////////////////////////////////////////////////
  std::string src_loc = asset_data._src_loc;
  std::string dst_loc = asset_data._dst_loc;
  
  // Resolve source URL and location info
  URL source_url;
  locationinfo_ptr_t location_info;
  
  printf("[AssetFetcher] Resolving source location: %s\n", src_loc.c_str());
  
  if (src_loc.find("<") == 0) {
    location_info = _config.resolveLocation(src_loc);
    if (location_info) {
      source_url = location_info->url;
      printf("[AssetFetcher] Resolved base URL: %s\n", source_url.toString().c_str());
      if (!asset_data._filename.empty()) {
        source_url = source_url / asset_data._filename;
        printf("[AssetFetcher] Full URL with filename: %s\n", source_url.toString().c_str());
      }
    }
  } else {
    source_url = URL(src_loc);
    printf("[AssetFetcher] Direct URL: %s\n", source_url.toString().c_str());
  }
  
  // Resolve destination path
  file::Path dest_path = _config.resolvePath(dst_loc);
  printf("[AssetFetcher] dst_loc: %s -> dest_path: %s\n", dst_loc.c_str(), dest_path.c_str());
  
  // Setup cache directory
  file::Path cache_dir = file::Path::stage_dir() / "assetcache";
  // Create directory if it doesn't exist
  mkdir(cache_dir.c_str(), 0755);
  
  // Check cache for this asset
  file::Path cache_file = cache_dir / asset_data._filename;
  bool need_download = true;
  
  if (!asset_data._md5.empty() && FileEnv::DoesFileExist(cache_file)) {
    // Check if cached file has correct MD5
    if (verifyMD5(cache_file, asset_data._md5)) {
      printf("[AssetFetcher] Cache hit for %s (MD5 verified)\n", asset_id.c_str());
      need_download = false;
    } else {
      printf("[AssetFetcher] Cache file exists but MD5 mismatch, re-downloading\n");
      // Remove invalid cached file
      std::remove(cache_file.c_str());
    }
  }
  
  if (need_download) {
    // Create download with unique temp file per asset
    file::Path temp_file = file::Path::mktemp(asset_id + "_", "_" + asset_data._filename);
    auto download = _download_manager->download(source_url, temp_file);
  
    // Apply API key if available
    if (location_info) {
    // Extract location key from src_loc for env var lookup
    std::string location_key;
    if (src_loc.find("<") == 0) {
      size_t end_pos = src_loc.find(">");
      if (end_pos != std::string::npos) {
        location_key = src_loc.substr(1, end_pos - 1);
      }
    }
    
    printf("[AssetFetcher] Location key: %s\n", location_key.c_str());
    std::string effective_key = location_info->getEffectiveApiKey(location_key);
    if (!effective_key.empty()) {
      printf("[AssetFetcher] Setting API key (length=%zu)\n", effective_key.length());
      download->setApiKey(effective_key);
    } else {
      printf("[AssetFetcher] No API key found for location\n");
    }
    
    // Apply disable_cert_check if set
    if (location_info->disable_cert_check) {
      printf("[AssetFetcher] Disabling certificate check for this download\n");
      download->_ignore_tls_errors = true;
    }
  } else {
    printf("[AssetFetcher] No location info found for URL\n");
  }
  
  ///////////////////////////////////////////////////////////
  // Set up download callbacks
  ///////////////////////////////////////////////////////////
  download->_on_progress._item = [this, asset_id](size_t downloaded, size_t total) {
    if (_on_asset_progress._item) {
      _on_asset_progress._item(asset_id, downloaded, total);
    }
  };
  
  // Capture necessary data for post-processing
  auto post_process = [this, asset_id, asset_data, dest_path, cache_dir, on_complete](bool download_success, const file::Path& temp_file) {
    // Queue post-processing on the opq
    _download_manager->_work_queue->enqueue([this, asset_id, asset_data, dest_path, cache_dir, temp_file, download_success, on_complete]() {
      bool process_success = false;
      
      if (download_success) {
        // Verify MD5 of downloaded encrypted file
        if (!asset_data._md5.empty()) {
          if (!verifyMD5(temp_file, asset_data._md5)) {
            printf("MD5 verification failed for downloaded file %s\n", asset_id.c_str());
            if (_on_asset_complete._item) {
              _on_asset_complete._item(asset_id, false);
            }
            on_complete();
            return;
          }
        }
        
        // Save to cache (encrypted file)
        file::Path cache_file = cache_dir / asset_data._filename;
        std::ifstream src(temp_file.c_str(), std::ios::binary);
        std::ofstream dst(cache_file.c_str(), std::ios::binary);
        if (src && dst) {
          dst << src.rdbuf();
          printf("[AssetFetcher] Cached downloaded file to %s\n", cache_file.c_str());
        }
        src.close();
        dst.close();
        
        // Process based on asset type
        if (asset_data._type == "asset_pak") {
          process_success = processAssetPak(asset_id, asset_data, temp_file, dest_path);
        } else if (asset_data._type == "asset") {
          process_success = processAsset(asset_id, asset_data, temp_file, dest_path);
        }
      }
      
      if (_on_asset_complete._item) {
        _on_asset_complete._item(asset_id, process_success);
      }
      
      on_complete();
    });
  };
  
  download->_on_complete._item = [post_process](bool success, const file::Path& path) {
    post_process(success, path);
  };
  
  download->_on_failure._item = [this, asset_id, post_process](const std::string& error) {
    printf("Download failed for %s: %s\n", asset_id.c_str(), error.c_str());
    post_process(false, file::Path());
  };
  } else {
    // Use cached file - still need to decrypt and untar
    // MD5 verification will happen after decryption in processAssetPak
    printf("[AssetFetcher] Using cached file for %s\n", asset_id.c_str());
    _download_manager->_work_queue->enqueue([this, asset_id, asset_data, dest_path, cache_file, on_complete]() {
      bool process_success = false;
      
      // Process based on asset type
      if (asset_data._type == "asset_pak") {
        process_success = processAssetPak(asset_id, asset_data, cache_file, dest_path);
      } else if (asset_data._type == "asset") {
        process_success = processAsset(asset_id, asset_data, cache_file, dest_path);
      }
      
      if (_on_asset_complete._item) {
        _on_asset_complete._item(asset_id, process_success);
      }
      
      on_complete();
    });
  }
}

bool AssetFetcher::processAssetPak(const std::string& asset_id,
                                  const AssetManifest::AssetEntry& asset_data,
                                  const file::Path& temp_file,
                                  const file::Path& dest_path) {
  // Decrypt if needed
  printf("Processing asset_pak<%s> temp_file<%s>\n", asset_id.c_str(), temp_file.c_str());
  file::Path decrypted_file = temp_file;
  auto ns_it = _config._namespace_keys.find(asset_data._namespace);
  if (ns_it != _config._namespace_keys.end()) {
    // Use mktemp for unique decrypted file name
    decrypted_file = file::Path::mktemp(asset_id + "_dec_", ".tar");
    printf("Decrypting asset_pak %s\n", asset_id.c_str());
    if (!decryptFile(temp_file, decrypted_file, ns_it->second)) {
      printf("Decryption failed for %s\n", asset_id.c_str());
      std::remove(temp_file.c_str());
      return false;
    }
  }
  
  // Extract tar
  printf("Extracting asset_pak<%s> to dest_path<%s>\n", asset_id.c_str(), dest_path.c_str() );
  bool success = extractTar(decrypted_file, dest_path);
  
  // Clean up temporary files
  std::remove(decrypted_file.c_str());
  if (decrypted_file != temp_file) {
    std::remove(temp_file.c_str());
  }
  
  return success;
}

bool AssetFetcher::processAsset(const std::string& asset_id,
                               const AssetManifest::AssetEntry& asset_data,
                               const file::Path& temp_file,
                               const file::Path& dest_path) {
  // Single file asset
  file::Path final_dest = dest_path;
  bool success = false;
  
  // Decrypt if needed
  auto ns_it = _config._namespace_keys.find(asset_data._namespace);
  if (ns_it != _config._namespace_keys.end()) {
    printf("Decrypting asset %s\n", asset_id.c_str());
    success = decryptFile(temp_file, final_dest, ns_it->second);
  } else {
    // Just move the file
    std::ifstream src(temp_file.c_str(), std::ios::binary);
    std::ofstream dst(final_dest.c_str(), std::ios::binary);
    if (src && dst) {
      dst << src.rdbuf();
      success = true;
    }
  }
  
  std::remove(temp_file.c_str());
  return success;
}

// Legacy blocking fetch implementation removed - use queueAssetFetch instead

////////////////////////////////////////////////////////////////////////////////

bool AssetFetcher::verifyMD5(const file::Path& file, const std::string& expected_md5) {
  // Read file and calculate MD5
  std::ifstream ifs(file.c_str(), std::ios::binary);
  if (!ifs.is_open()) {
    return false;
  }
  
  // Calculate MD5 hash of file contents
  CMD5 hasher;
  
  // Read file in chunks and update hash
  const size_t buffer_size = 8192;
  char buffer[buffer_size];
  while (ifs.good()) {
    ifs.read(buffer, buffer_size);
    size_t bytes_read = ifs.gcount();
    if (bytes_read > 0) {
      hasher.update((unsigned char*)buffer, bytes_read);
    }
  }
  ifs.close();
  
  hasher.finalize();
  Md5Sum result = hasher.Result();
  std::string calculated_md5 = result.hex_digest();
  
  // Compare with expected MD5
  bool match = (calculated_md5 == expected_md5);
  if (!match) {
    printf("[AssetFetcher] MD5 mismatch: expected=%s, calculated=%s\n", 
           expected_md5.c_str(), calculated_md5.c_str());
  }
  
  return match;
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
  
  // Build command line with -C flag to specify extraction directory
  std::string cmd = "tar xvf ";
  cmd += tar_file.c_str();
  cmd += " -C ";
  cmd += dest_dir.c_str();
  
  printf("[AssetFetcher] Tar extract command: %s\n", cmd.c_str());
  
  spawner.mCommandLine = cmd;
  spawner.spawnSynchronous();
  
  return spawner.mExecRet == 0;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog