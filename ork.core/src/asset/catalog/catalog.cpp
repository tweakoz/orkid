////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/uploader.h>
#include <ork/asset/catalog/request.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <ork/util/logger.h>
#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <functional>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <boost/filesystem.hpp>
#include "catalog_impl.h"

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// AssetCatalog
////////////////////////////////////////////////////////////////

// Static global instance - thread-safe initialization
assetcatalog_ptr_t AssetCatalog::globalInstance() {
  static std::once_flag init_flag;
  static assetcatalog_ptr_t global_catalog;
  
  std::call_once(init_flag, []() {
    // Load global configs first
    auto config_space = AssetConfigSpace::loadGlobalConfigs();
    // Create catalog with the loaded config space
    global_catalog = std::make_shared<AssetCatalog>(config_space);
    AssetCatalog::loadFromGlobalManifests(global_catalog);
  });
  
  return global_catalog;
}

AssetCatalog::AssetCatalog() {
  auto impl = _impl.makeShared<CatalogImpl>(this);
  
  // Initialize cache directory
  _cache_dir = file::Path::stage_dir() / "assetcache";
  
  // Ensure cache directory structure exists
  boost::filesystem::create_directories(_cache_dir.toBFS());
  boost::filesystem::create_directories(getEncryptedDir().toBFS());
  boost::filesystem::create_directories(getChunksDir().toBFS());
  boost::filesystem::create_directories(getReceiptsDir().toBFS());
  boost::filesystem::create_directories(getTempDir().toBFS());
}

AssetCatalog::AssetCatalog(assetconfigspace_ptr_t space) {
  auto impl = _impl.makeShared<CatalogImpl>(this);
  
  // Initialize cache directory
  _cache_dir = file::Path::stage_dir() / "assetcache";
  
  // Ensure cache directory structure exists
  boost::filesystem::create_directories(_cache_dir.toBFS());
  boost::filesystem::create_directories(getEncryptedDir().toBFS());
  boost::filesystem::create_directories(getChunksDir().toBFS());
  boost::filesystem::create_directories(getReceiptsDir().toBFS());
  boost::filesystem::create_directories(getTempDir().toBFS());
  impl->_config_space = space;
}

AssetCatalog::~AssetCatalog() {
  auto impl = _impl.getShared<CatalogImpl>();
  impl->_shutdown_requested = true;
}

/////////////////////////////////////////////////////////////////////////////////

void AssetCatalog::requestShutdown() {
  auto impl = _impl.getShared<CatalogImpl>();
  impl->requestShutdown();
}

void AssetCatalog::drainPendingOperations() {
  auto impl = _impl.getShared<CatalogImpl>();
  impl->drainPendingOperations();
}

/////////////////////////////////////////////////////////////////////////////////

bool AssetCatalog::hasAsset(const assetid_t& fq_asset_id) const {
  auto impl     = _impl.getShared<CatalogImpl>();
  auto location = impl->locateAsset(fq_asset_id);
  return location != nullptr;
}

/////////////////////////////////////////////////////////////////////////////////

assetindexentry_ptr_t AssetCatalog::findAssetIndexEntry(const assetid_t& fq_asset_id) const {
  auto impl               = _impl.getShared<CatalogImpl>();
  assetindexentry_ptr_t result = nullptr;
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    auto it = state._entries_by_assetid.find(fq_asset_id);
    if (it != state._entries_by_assetid.end()) {
      result = it->second;
    }
  });
  return result;
}

/////////////////////////////////////////////////////////////////////////////////

assetentry_ptr_t AssetCatalog::findAssetEntry(const assetid_t& fq_asset_id) const {
  auto index_entry = findAssetIndexEntry(fq_asset_id);
  assetentry_ptr_t result = nullptr;
  if (index_entry) {
    result = index_entry->_entry;
  }
  return result;
}

/////////////////////////////////////////////////////////////////////////////////

assetindexentry_ptr_t AssetCatalog::_addNewAssetIndexEntry(const assetid_t& fq_asset_id) {
  auto impl               = _impl.getShared<CatalogImpl>();
  assetindexentry_ptr_t result = nullptr;
  impl->_state.atomicOp([&](CatalogImpl::CatalogState& unlocked) {
    auto it = unlocked._entries_by_assetid.find(fq_asset_id);
    if (it != unlocked._entries_by_assetid.end()) {
      OrkAssert(false); // Entry already exists
    }
    else{
      // Asset not found, create a new entry with minimal info
      auto new_entry = std::make_shared<AssetEntry>();
      auto [ns_id, asset_id] = impl->parseAssetId(fq_asset_id);
      new_entry->_namespace = ns_id;
      new_entry->_id = asset_id;
      new_entry->_storage_hash = ""; // Unknown at this point
      new_entry->_local_loc = "";    // Unknown at this point
      result = std::make_shared<AssetIndexEntry>();
      result->_entry = new_entry;
      result->_namespace_id = ns_id;
      unlocked._entries_by_assetid[fq_asset_id] = result;
    }
  });
  return result;
}
////////////////////////////////////////////////////////////////
// Asset Queries
////////////////////////////////////////////////////////////////

assetid_list_t AssetCatalog::listAssets(const std::string& pattern) const {
  auto impl = _impl.getShared<CatalogImpl>();
  assetid_list_t result;

  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    for (const auto& [_asset_id, entry] : state._entries_by_assetid) {
      bool matches = false;

      if (pattern.empty() || pattern == "*") {
        // Empty pattern or single wildcard matches everything
        matches = true;
      } else if (pattern.find('*') != std::string::npos) {
        // Handle wildcard patterns
        if (pattern.back() == '*') {
          // Pattern ends with wildcard - prefix match
          std::string prefix = pattern.substr(0, pattern.length() - 1);
          matches            = _asset_id.find(prefix) == 0;
        } else {
          // TODO: Handle more complex wildcard patterns
          matches = false;
        }
      } else {
        // No wildcards - check if pattern is contained in _asset_id
        matches = _asset_id.find(pattern) != std::string::npos;
      }

      if (matches) {
        result.push_back(_asset_id);
      }
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////

assetid_list_t AssetCatalog::listAssetsInNamespace(const namespaceid_t& namespace_id) const {
  auto impl = _impl.getShared<CatalogImpl>();
  assetid_list_t result;

  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    for (const auto& [_asset_id, entry] : state._entries_by_assetid) {
      if (entry->_namespace_id == namespace_id) {
        result.push_back(_asset_id);
      }
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////

std::string AssetCatalog::dumpAllAssetFQIDs() const {
  auto impl = _impl.getShared<CatalogImpl>();
  std::string result;
  
  // Helper function to recursively dump assets from a namespace
  std::function<void(assetnamespace_ptr_t, int)> dumpNamespace = 
    [&](assetnamespace_ptr_t ns, int depth) {
      if (!ns) return;
      
      // Get all assets in this namespace (non-container namespaces)
      if (!ns->isContainerOnly()) {
        impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
          for (const auto& [fq_asset_id, index_entry] : state._entries_by_assetid) {
            if (index_entry->_namespace_id == ns->_id) {
              // Use the AssetEntry's buildFullyQualifiedId method
              std::string fqid = index_entry->_entry->buildFullyQualifiedId();
              result += fqid + "\n";
            }
          }
        });
      }
      
      // Recursively process children (sort for consistent output)
      std::vector<std::pair<std::string, assetnamespace_ptr_t>> sorted_children;
      for (const auto& [name, child] : ns->_children) {
        sorted_children.emplace_back(name, child);
      }
      std::sort(sorted_children.begin(), sorted_children.end());
      
      for (const auto& [name, child] : sorted_children) {
        dumpNamespace(child, depth + 1);
      }
    };
  
  // Start from root namespace
  dumpNamespace(impl->_root_namespace, 0);
  
  return result;
}

////////////////////////////////////////////////////////////////
// Manifest Factory
////////////////////////////////////////////////////////////////

assetmanifest_ptr_t AssetCatalog::createManifest(
    assetcatalog_ptr_t catalog,
    const std::string& id,
    const std::string& version,
    const namespaceid_t& namespace_id,
    const file::Path& file) {
  
  auto impl = catalog->_impl.getShared<CatalogImpl>();
  
  // Create new manifest with parent catalog reference
  auto manifest = std::make_shared<AssetManifest>(std::weak_ptr<AssetCatalog>(catalog));
  
  // Set basic properties
  manifest->setNamespace(namespace_id);
  manifest->setVersion(version);
  
  // UUID is automatically gene_rated in AssetManifest constructor
  
  // Register namespace if it doesn't exist
  auto ns = catalog->_mergeNamespace(namespace_id);
  
  // Add manifest to catalog
  catalog->_addManifest(manifest);
  
  // TODO: Track file path for later writeToDisk
  
  return manifest;
}

////////////////////////////////////////////////////////////////
// Configuration
////////////////////////////////////////////////////////////////

void AssetCatalog::setConfigSpace(assetconfigspace_ptr_t space) {
  auto impl = _impl.getShared<CatalogImpl>();
  impl->_config_space = space;
}

assetconfigspace_ptr_t AssetCatalog::getConfigSpace() const {
  auto impl = _impl.getShared<CatalogImpl>();
  return impl->_config_space;
}

void AssetCatalog::setDownloadManager(downloadmanager_ptr_t mgr) {
  auto impl               = _impl.getShared<CatalogImpl>();
  impl->_download_manager = mgr;
}

////////////////////////////////////////////////////////////////
// Utility Methods
////////////////////////////////////////////////////////////////

std::pair<std::string, std::string> CatalogImpl::parseAssetId(const assetid_t& fq_asset_id) const {
  // Find the last occurrence of :: to sepa_rate namespace path from asset path
  size_t last_sep = fq_asset_id.rfind("|");
  if (last_sep != std::string::npos) {
    // Everything before last :: is the namespace path
    // Everything after last :: is the asset path
    return {fq_asset_id.substr(0, last_sep), fq_asset_id.substr(last_sep + 1)};
  }
  // No :: found, so no namespace - asset is in root namespace
  return {"", fq_asset_id};
}

////////////////////////////////////////////////////////////////

assetid_t AssetCatalog::buildAssetId(const namespaceid_t& namespace_id, const std::string& asset_path) {
  if (namespace_id.empty()) {
    return asset_path;
  }
  return namespace_id + "|" + asset_path;
}

////////////////////////////////////////////////////////////////
// Cache Directory Management
////////////////////////////////////////////////////////////////

void AssetCatalog::setCacheDir(const file::Path& dir) {
  _cache_dir = dir;
  // Recreate directory structure
  boost::filesystem::create_directories(_cache_dir.toBFS());
  boost::filesystem::create_directories(getEncryptedDir().toBFS());
  boost::filesystem::create_directories(getChunksDir().toBFS());
  boost::filesystem::create_directories(getReceiptsDir().toBFS());
  boost::filesystem::create_directories(getTempDir().toBFS());
}

file::Path AssetCatalog::getCacheDir() const {
  return _cache_dir;
}

file::Path AssetCatalog::getEncryptedDir() const {
  return _cache_dir / "enc";
}

file::Path AssetCatalog::getChunksDir() const {
  return _cache_dir / "enc" / "chunks";
}

file::Path AssetCatalog::getReceiptsDir() const {
  return _cache_dir / "receipts";
}

file::Path AssetCatalog::getTempDir() const {
  return _cache_dir / "temp";
}

////////////////////////////////////////////////////////////////////////////////
// URL Generation Methods - Single Source of Truth
// Non-chunked asset: {xfer_url}/{storage_hash}.enc
////////////////////////////////////////////////////////////////////////////////

URL AssetCatalog::getAssetUploadURL( const AssetEntry* entry, //
                                     locationinfo_ptr_t location) const { //
  return location->_upload_url / (entry->_storage_hash + ".enc");
}

URL AssetCatalog::getAssetDownloadURL(const AssetEntry* entry, locationinfo_ptr_t location) const {
  return location->_download_url / (entry->_storage_hash + ".enc");
}

////////////////////////////////////////////////////////////////////////////////
// URL Generation Methods (chunks) - Single Source of Truth
// Individual chunk: {xfer_url}/{chunk_hash}.chunk.{index:04zu}
////////////////////////////////////////////////////////////////////////////////

std::string AssetCatalog::getChunkFilename(std::string basename,        //
                                           size_t chunk_index) const {  //
  return FormatString("%s.chunk.%04zu", basename.c_str(), chunk_index);
}

////////////////////////////////////////////////////////////////
// Asset Request State Management (Flyweight)
////////////////////////////////////////////////////////////////

fetchrequest_ptr_t AssetCatalog::_mergeRequest(assetfqid_ptr_t fqid) {
  auto impl = _impl.getShared<CatalogImpl>();
  fetchrequest_ptr_t request;

  impl->_active_handles.atomicOp([&](assethandle_map_t& unlocked) {
    auto aid = fqid->_original_fqid;
    auto it = unlocked.find(aid);
    if (it != unlocked.end()) {
      // Return existing request (flyweight pattern)
      request = it->second;
    } else {
      // Create new request
      request            = std::make_shared<FetchRequest>();
      unlocked[aid] = request;
      request->_fqid = fqid;
    }
    // Increment refcount for this access
    request->_refcount.fetch_add(1);
  });

  return request;
}

////////////////////////////////////////////////////////////////

assetnamespace_ptr_t AssetCatalog::_mergeNamespace(const namespaceid_t& namespace_id) {
  auto impl = _impl.getShared<CatalogImpl>();
  assetnamespace_ptr_t ns;

  impl->_state.atomicOp([&](CatalogImpl::CatalogState& state) {
    auto it = state._nodes_by_namespace.find(namespace_id);
    if (it != state._nodes_by_namespace.end()) {
      // Return existing namespace (flyweight pattern)
      ns = it->second;
      return;
    }

    // Split namespace path into components (e.g., "game::textures::characters" -> ["game", "textures", "characters"])
    std::vector<std::string> components;
    size_t start = 0;
    size_t end = 0;
    
    while ((end = namespace_id.find("|", start)) != std::string::npos) {
      if (end > start) {
        components.push_back(namespace_id.substr(start, end - start));
      }
      start = end + 2; // Skip "|"
    }
    
    // Add the last component
    if (start < namespace_id.length()) {
      components.push_back(namespace_id.substr(start));
    }

    // Create hierarchy from root down
    assetnamespace_ptr_t current = impl->_root_namespace;
    std::string current_path = "";
    
    for (size_t i = 0; i < components.size(); ++i) {
      const std::string& component = components[i];
      
      // Build full path for this level
      if (current_path.empty()) {
        current_path = component;
      } else {
        current_path += "|" + component;
      }
      
      // Check if this level already exists
      auto level_it = state._nodes_by_namespace.find(current_path);
      if (level_it != state._nodes_by_namespace.end()) {
        current = level_it->second;
      } else {
        // Create new namespace for this level
        auto new_ns = std::make_shared<AssetNamespace>(component);
        new_ns->_full_path = current_path;
        new_ns->_parent = current;
        
        // Mark intermediate levels as container-only (except the final one)
        if (i < components.size() - 1) {
          new_ns->setContainerOnly(true);
        }
        
        // Add to parent's children
        current->_children[component] = new_ns;
        
        // Store in flyweight map
        state._nodes_by_namespace[current_path] = new_ns;
        
        current = new_ns;
      }
    }
    
    ns = current;
  });

  return ns;
}

////////////////////////////////////////////////////////////////////////////////

void AssetCatalog::repackage() {
  auto impl = _impl.getShared<CatalogImpl>();
  
  // Collect all manifests to repackage
  std::vector<std::pair<namespaceid_t, assetmanifest_ptr_t>> manifests_to_repackage;
  
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    for (const auto& [namespace_id, manifest_list] : state._manifests_by_namespace) {
      for (const auto& manifest : manifest_list) {
        manifests_to_repackage.push_back({namespace_id, manifest});
      }
    }
  });
  
  logchan_catalog->log("Repackaging catalog with %zu manifests...", manifests_to_repackage.size());
  
  // Repackage each manifest (outside the lock)
  for (const auto& [namespace_id, manifest] : manifests_to_repackage) {
    logchan_catalog->log("  Repackaging manifest for namespace: %s", namespace_id.c_str());
    
    if (manifest) {
      manifest->repackage();
    }
  }
  
  logchan_catalog->log("Catalog repackaging complete.");
}

////////////////////////////////////////////////////////////////////////////////

uploadreceipt_ptr_t AssetCatalog::uploadNamespace(
    const namespaceid_t& namespace_id,
    asset_completed_callback_t on_asset_completed) {
  logchan_catalog->log("Starting upload for namespace: %s", namespace_id.c_str());
  
  auto impl = _impl.getShared<CatalogImpl>();
  
  // Get the config space to determine destination
  auto config_space = getConfigSpace();
  if (!config_space) {
    logchan_catalog->log("ERROR: No config space available for catalog upload");
    return nullptr;
  }
  
  // Get merged config (should have namespace configuration)
  auto config = config_space->merged();
  if (!config) {
    logchan_catalog->log("ERROR: No merged config found in config space");
    return nullptr;
  }
  
  // Check if namespace has remote location configured
  auto upload_location = config->getRemoteLocationForNamespace(namespace_id);
  if (!upload_location) {
    logchan_catalog->log("ERROR: No upload location configured for namespace: %s", namespace_id.c_str());
    return nullptr;
  }
  
  // Find all manifests for this namespace
  std::vector<assetmanifest_ptr_t> manifests_to_upload;
  
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    auto it = state._manifests_by_namespace.find(namespace_id);
    if (it != state._manifests_by_namespace.end()) {
      for (const auto& manifest : it->second) {
        if (manifest) {
          manifests_to_upload.push_back(manifest);
        }
      }
    }
  });
  
  if (manifests_to_upload.empty()) {
    logchan_catalog->log("WARNING: No manifests found for namespace: %s", namespace_id.c_str());
    return nullptr;
  }
  
  logchan_catalog->log("Found %zu manifests to upload for namespace: %s", manifests_to_upload.size(), namespace_id.c_str());
  
  // Create combined receipt for all manifests in namespace
  auto combined_receipt = std::make_shared<UploadReceipt>();
  combined_receipt->upload_id = "catalog_" + namespace_id + "_" + std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
  combined_receipt->timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  combined_receipt->success = true;
  combined_receipt->status_message = "Starting catalog upload for namespace: " + namespace_id;
  combined_receipt->total_files = 0;
  combined_receipt->bytes_uploaded = 0;
  
  // Upload each manifest and aggregate results
  for (size_t i = 0; i < manifests_to_upload.size(); i++) {
    const auto& manifest = manifests_to_upload[i];
    logchan_catalog->log("  Uploading manifest %zu/%zu: %s", i + 1, manifests_to_upload.size(), manifest->getManifestId().c_str());
    
    try {
      auto manifest_receipt = manifest->upload(*config, upload_location, on_asset_completed);
      
      if (manifest_receipt) {
        // Aggregate results
        combined_receipt->total_files += manifest_receipt->total_files;
        combined_receipt->bytes_uploaded += manifest_receipt->bytes_uploaded;
        
        if (!manifest_receipt->success) {
          combined_receipt->success = false;
          combined_receipt->status_message += "\n  Manifest " + manifest->getManifestId() + " failed: " + manifest_receipt->status_message;
        } else {
          logchan_catalog->log("    Manifest uploaded: %zu files, %zu bytes", manifest_receipt->total_files, manifest_receipt->bytes_uploaded);
        }
      } else {
        combined_receipt->success = false;
        combined_receipt->status_message += "\n  Manifest " + manifest->getManifestId() + " failed: no receipt returned";
      }
    } catch (const std::exception& e) {
      combined_receipt->success = false;
      combined_receipt->status_message += "\n  Manifest " + manifest->getManifestId() + " failed: " + e.what();
      logchan_catalog->log("    ERROR: Manifest upload failed: %s", e.what());
    }
  }
  
  if (combined_receipt->success) {
    combined_receipt->status_message = "Successfully uploaded namespace '" + namespace_id + "': " + 
                                      std::to_string(combined_receipt->total_files) + " files, " + 
                                      std::to_string(combined_receipt->bytes_uploaded) + " bytes";
    logchan_catalog->log("Namespace upload SUCCESS - namespace: %s, files: %zu, bytes: %zu", 
                         namespace_id.c_str(), combined_receipt->total_files, combined_receipt->bytes_uploaded);
  } else {
    logchan_catalog->log("ERROR: Namespace upload FAILED - namespace: %s, error: %s", 
                         namespace_id.c_str(), combined_receipt->status_message.c_str());
  }
  
  logchan_catalog->log("Completed upload for namespace: %s", namespace_id.c_str());
  return combined_receipt;
}

////////////////////////////////////////////////////////////////////////////////

uploadreceipt_ptr_t AssetCatalog::uploadAsset(
    const assetid_t& fq_asset_id,
    chunk_completed_callback_t on_chunk_completed) {
  logchan_catalog->log("Starting upload for asset: %s", fq_asset_id.c_str());
  
  auto impl = _impl.getShared<CatalogImpl>();
  
  // Parse the fully qualified asset ID to get namespace and asset ID
  size_t separator_pos = fq_asset_id.find('|');
  if (separator_pos == std::string::npos) {
    logchan_catalog->log("ERROR: Invalid fully qualified asset ID (missing namespace separator '|'): %s", fq_asset_id.c_str());
    return nullptr;
  }
  
  namespaceid_t namespace_id = fq_asset_id.substr(0, separator_pos);
  assetid_t asset_id = fq_asset_id.substr(separator_pos + 1);
  
  // Get the config space to determine destination
  auto config_space = getConfigSpace();
  if (!config_space) {
    logchan_catalog->log("ERROR: No config space available for catalog upload");
    return nullptr;
  }
  
  // Get merged config (should have namespace configuration)
  auto config = config_space->merged();
  if (!config) {
    logchan_catalog->log("ERROR: No merged config found in config space");
    return nullptr;
  }
  
  // Check if namespace has remote location configured
  auto upload_location = config->getRemoteLocationForNamespace(namespace_id);
  if (!upload_location) {
    logchan_catalog->log("ERROR: No upload location configured for namespace: %s", namespace_id.c_str());
    return nullptr;
  }
  
  // Find the manifest containing this asset
  assetmanifest_ptr_t target_manifest;
  assetentry_ptr_t target_asset;
  
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    auto it = state._manifests_by_namespace.find(namespace_id);
    if (it != state._manifests_by_namespace.end()) {
      for (const auto& manifest : it->second) {
        if (manifest) {
          // Get all assets and search for the one we want
          const auto& assets = manifest->getAssets();
          auto asset_it = assets.find(asset_id);
          if (asset_it != assets.end()) {
            target_manifest = manifest;
            target_asset = asset_it->second;
            break;
          }
        }
      }
    }
  });
  
  if (!target_manifest || !target_asset) {
    logchan_catalog->log("ERROR: Asset not found: %s", fq_asset_id.c_str());
    return nullptr;
  }
  
  logchan_catalog->log("Found asset '%s' in manifest '%s'", asset_id.c_str(), target_manifest->getManifestId().c_str());
  
  // Create receipt for single asset upload
  auto receipt = std::make_shared<UploadReceipt>();
  receipt->upload_id = "asset_" + fq_asset_id + "_" + std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
  receipt->timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  receipt->success = false;
  receipt->status_message = "Starting asset upload: " + fq_asset_id;
  receipt->total_files = 0;
  receipt->bytes_uploaded = 0;
  
  try {
    // Upload the single asset using AssetEntry's upload method (same as manifest does)
    auto upload_receipt = target_asset->upload(*config, upload_location, on_chunk_completed);
    
    if (upload_receipt) {
      // Copy results from upload receipt
      receipt->success = upload_receipt->success;
      receipt->total_files = upload_receipt->total_files;
      receipt->successful_files = upload_receipt->successful_files;
      receipt->bytes_uploaded = upload_receipt->bytes_uploaded;
      receipt->failed_files = upload_receipt->failed_files;
      
      if (upload_receipt->success) {
        receipt->status_message = "Successfully uploaded asset '" + fq_asset_id + "': " + 
                                 std::to_string(receipt->bytes_uploaded) + " bytes";
        logchan_catalog->log("Asset upload SUCCESS - asset: %s, bytes: %zu", 
                           fq_asset_id.c_str(), receipt->bytes_uploaded);
      } else {
        receipt->status_message = "Failed to upload asset '" + fq_asset_id + "': " + upload_receipt->status_message;
        logchan_catalog->error("Asset upload FAILED - asset: %s, error: %s",
                               fq_asset_id.c_str(), upload_receipt->status_message.c_str());
      }
    } else {
      receipt->status_message = "Upload returned no receipt";
      logchan_catalog->error("Asset upload failed - no receipt returned");
    }
  } catch (const std::exception& e) {
    receipt->status_message = "Asset upload failed: " + std::string(e.what());
    logchan_catalog->error("Asset upload exception: %s", e.what());
  }
  
  logchan_catalog->log("Completed upload for asset: %s", fq_asset_id.c_str());
  return receipt;
}

////////////////////////////////////////////////////////////////////////////////

upload_result_map_t AssetCatalog::uploadAllNamespaces(
    namespace_completed_callback_t on_namespace_completed) {
  logchan_catalog->log("Starting upload for all namespaces");
  
  auto impl = _impl.getShared<CatalogImpl>();
  upload_result_map_t results;
  
  // Collect all namespace IDs
  std::vector<namespaceid_t> namespace_ids;
  
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    for (const auto& [namespace_id, manifest_list] : state._manifests_by_namespace) {
      if (!manifest_list.empty()) {
        namespace_ids.push_back(namespace_id);
      }
    }
  });
  
  logchan_catalog->log("Found %zu namespaces to upload", namespace_ids.size());
  
  // Upload each namespace
  for (const auto& namespace_id : namespace_ids) {
    logchan_catalog->log("Uploading namespace: %s", namespace_id.c_str());

    try {
      // Pass nullptr for asset callback since we're tracking at namespace level
      auto receipt = uploadNamespace(namespace_id, nullptr);
      results[namespace_id] = receipt;

      if (receipt && receipt->success) {
        logchan_catalog->log("Namespace '%s' uploaded successfully", namespace_id.c_str());

        // Invoke namespace completion callback
        if (on_namespace_completed) {
          on_namespace_completed(namespace_id);
        }
      } else {
        logchan_catalog->log("ERROR: Namespace '%s' upload failed", namespace_id.c_str());
      }
    } catch (const std::exception& e) {
      logchan_catalog->log("ERROR: Exception uploading namespace '%s': %s", namespace_id.c_str(), e.what());
      
      // Create failure receipt
      auto failed_receipt = std::make_shared<UploadReceipt>();
      failed_receipt->upload_id = "failed_" + namespace_id;
      failed_receipt->timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      failed_receipt->success = false;
      failed_receipt->status_message = "Exception: " + std::string(e.what());
      results[namespace_id] = failed_receipt;
    }
  }
  
  // Summary
  size_t successful = 0;
  size_t failed = 0;
  size_t total_files = 0;
  size_t _total_bytes = 0;
  
  for (const auto& [namespace_id, receipt] : results) {
    if (receipt && receipt->success) {
      successful++;
      total_files += receipt->total_files;
      _total_bytes += receipt->bytes_uploaded;
    } else {
      failed++;
    }
  }
  
  logchan_catalog->log("Upload all namespaces summary - total: %zu, successful: %zu, failed: %zu, files: %zu, bytes: %zu",
                       namespace_ids.size(), successful, failed, total_files, _total_bytes);
  
  logchan_catalog->log("Completed upload for all namespaces");
  return results;
}

} // namespace ork::asset::catalog