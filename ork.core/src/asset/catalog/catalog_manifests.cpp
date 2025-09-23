///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <ork/util/logger.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include "catalog_impl.h"

namespace ork::asset::catalog {

logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// Manifest Management
////////////////////////////////////////////////////////////////

void AssetCatalog::loadManifestsFromPath(const file::Path& path) {
  namespace fs = boost::filesystem;

  if (!path.doesPathExist()) {
    logchan_catalog->log("WARNING: Manifest path does not exist: %s", path.c_str());
    return;
  }

  try {
    fs::path boost_path = path.toBFS();

    if (fs::is_directory(boost_path)) {
      // Scan directory for .json files
      for (fs::directory_iterator it(boost_path), end; it != end; ++it) {
        if (fs::is_regular_file(it->status()) && it->path().extension() == ".json") {

          file::Path manifest_file;
          manifest_file.fromBFS(it->path());

          // Load manifest from JSON file
          auto manifest = AssetManifest::loadFromFile(manifest_file);
          if (manifest) {
            _addManifest(manifest);
            // Already logged below
          } else {
            logchan_catalog->log("WARNING: Failed to load manifest: %s", manifest_file.c_str());
          }
        }
      }
    } else if (fs::is_regular_file(boost_path) && boost_path.extension() == ".json") {
      // Single manifest file
      auto manifest = AssetManifest::loadFromFile(path);
      if (manifest) {
        _addManifest(manifest);
        // Already logged via logchan_catalog
      } else {
        logchan_catalog->log("WARNING: Failed to load manifest: %s", path.c_str());
      }
    }
  } catch (const fs::filesystem_error& e) {
    logchan_catalog->log("ERROR: Filesystem error loading manifests from %s: %s", path.c_str(), e.what());
  }
}

/////////////////////////////////////////////////////////////////////////////////

void AssetCatalog::loadFromGlobalManifests(assetcatalog_ptr_t self) {
  // Get ORKID_ASSET_MANIFEST_DIRS environment variable
  const char* manifest_dirs_env = getenv("ORKID_ASSET_MANIFEST_DIRS");
  if (!manifest_dirs_env || strlen(manifest_dirs_env) == 0) {
    logchan_catalog->log("ORKID_ASSET_MANIFEST_DIRS not set");
    return;
  }

  logchan_catalog->log("building catalog from $ORKID_ASSET_MANIFEST_DIRS ...");

  std::string manifest_dirs_str(manifest_dirs_env);
  std::vector<std::string> manifest_dirs;

  // Split by colon (Unix path separator)
  size_t start = 0;
  size_t end   = manifest_dirs_str.find(':');
  while (end != std::string::npos) {
    std::string dir = manifest_dirs_str.substr(start, end - start);
    if (!dir.empty()) {
      manifest_dirs.push_back(dir);
    }
    start = end + 1;
    end   = manifest_dirs_str.find(':', start);
  }
  // Add the last directory
  std::string last_dir = manifest_dirs_str.substr(start);
  if (!last_dir.empty()) {
    manifest_dirs.push_back(last_dir);
  }

  // Loading logged below

  auto impl = self->_impl.getShared<CatalogImpl>();
  
  // Group manifests by directory for organized output
  std::map<std::string, std::vector<file::Path>> manifests_by_dir;

  // First pass: collect all manifest files grouped by directory
  for (const auto& manifest_dir : manifest_dirs) {
    file::Path dir_path(manifest_dir);

    if (!dir_path.doesPathExist()) {
      logchan_catalog->log("WARNING: Manifest directory does not exist: %s", dir_path.sanitize().c_str());
      continue;
    }

    namespace fs       = boost::filesystem;
    fs::path boost_dir = dir_path.toBFS();
    
    // Use sanitized path as key for grouping
    std::string sanitized_dir = dir_path.sanitize().toStdString();

    try {
      for (fs::directory_iterator it(boost_dir), end; it != end; ++it) {
        if (fs::is_regular_file(it->status()) && it->path().extension() == ".json") {
          std::string filename = it->path().filename().string();

          // Skip config.json files
          if (filename == "config.json") {
            continue;
          }

          file::Path manifest_file;
          manifest_file.fromBFS(it->path());
          manifests_by_dir[sanitized_dir].push_back(manifest_file);
        }
      }
    } catch (const fs::filesystem_error& e) {
      logchan_catalog->log("ERROR: Filesystem error loading manifests from %s: %s", 
                          dir_path.sanitize().c_str(), e.what());
    }
  }
  
  // Second pass: load manifests grouped by directory
  for (const auto& [dir, files] : manifests_by_dir) {
    if (!files.empty()) {
      logchan_catalog->log("Loading manifests from %s:", dir.c_str());
      for (const auto& manifest_file : files) {
        logchan_catalog->log("  - %s", manifest_file.getName().c_str());
        
        auto manifest = AssetManifest::loadFromFile(manifest_file);
        if (manifest) {
          self->_addManifest(manifest);
          manifest->_parent_catalog = self; // Set parent catalog for manifest
        } else {
          logchan_catalog->log("    WARNING: Failed to load");
        }
      }
    }
  }
  std::atomic<int> codec_counter = 0;
  // Register codecs for all namespaces found in configs
  if (impl->_config_space) {
    logchan_catalog->log("merging config:");

    auto merged_config = impl->_config_space->merged();

    logchan_catalog->log("registering codecs.");

    // Get all namespaces from merged config
    for (const auto& [namespace_id, namespace_info] : merged_config->_namespaces) {
      codec_counter.fetch_add(1);
      // Check if codec already registered
      bool needs_registration = false;
      impl->_state.atomicOp([&](CatalogImpl::CatalogState& state) {
        if (state._codecs_by_namespace.find(namespace_id) == state._codecs_by_namespace.end()) {
          needs_registration = true;
        }
      });

      if (needs_registration) {
        // Get encryption key for namespace
        std::string encryption_key = namespace_info->_encryption_key;

        if (!encryption_key.empty()) {
          // Use the public API to register codec
          auto op = [=,&codec_counter](){
            self->registerCodecWithPassword(namespace_id, encryption_key);
            codec_counter.fetch_sub(1);
          };
          opq::concurrentQueue()->enqueue(op);
          // Codec registration logged elsewhere if needed
        }
      }
    }
    while(codec_counter.load()>0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    logchan_catalog->log("codecs registered...");

  }
}

/////////////////////////////////////////////////////////////////////////////////

void AssetCatalog::_addManifest(assetmanifest_ptr_t manifest) {
  if (!manifest)
    return;

  auto impl               = _impl.getShared<CatalogImpl>();
  const auto namespace_id = manifest->getNamespace();
  // Manifest addition logged at higher level

  // Get or create flyweight namespace
  auto ns = _mergeNamespace(namespace_id);

  impl->_state.atomicOp([&](CatalogImpl::CatalogState& state) {
    // Add manifest to namespace's manifest list
    state._manifests_by_namespace[namespace_id].push_back(manifest);
    // Manifest pointer logging not needed
    // Add all assets to index
    const auto& assets = manifest->getAssets();
    for (const auto& [_asset_id, entry] : assets) {
      // Populate AssetEntry fields
      entry->_id            = _asset_id;    // Set the asset ID
      entry->_namespace     = namespace_id; // Set namespace string
      entry->_namespace_ptr = ns;           // Set namespace weak pointer

      auto index_entry = std::make_shared<AssetIndexEntry>();
      index_entry->_namespace_id = namespace_id;
      index_entry->_manifest     = manifest;
      index_entry->_entry        = entry;
      
      // Debug: Check what's in the entry
      if(0){
        printf("[DEBUG catalog_manifests] Indexing arena4k:\n");
        printf("  storage_hash: '%s'\n", entry->_storage_hash.c_str());
        printf("  content_hash: '%s'\n", entry->_content_hash.c_str());
        printf("  local_loc: '%s'\n", entry->_local_loc.c_str());
      }

      // Store with fully qualified ID
      std::string fq_asset_id                = buildAssetId(namespace_id, _asset_id);
      state._entries_by_assetid[fq_asset_id] = index_entry;
      // Asset entry addition logged at higher level
    }
  });

}

/////////////////////////////////////////////////////////////////////////////////

manifest_list_t AssetCatalog::manifestsForNamespace(const namespaceid_t& namespace_id) const {
  auto impl                  = _impl.getShared<CatalogImpl>();
  manifest_list_t result;
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    auto it = state._manifests_by_namespace.find(namespace_id);
    if (it != state._manifests_by_namespace.end() && !it->second.empty()) {
      result = it->second;
    }
  });
  return result;
}

} // namespace ork::asset::catalog
