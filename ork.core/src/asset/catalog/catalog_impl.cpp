#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/request.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <ork/util/logger.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <ork/util/password_provider.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "catalog_impl.h"
#include <nlohmann/json.hpp>

////////////////////////////////////////////////////////////////
// Internal Methods
////////////////////////////////////////////////////////////////

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->configureChannel("CATALOG", fvec3(0.5, 0.5, 0.8), true);

////////////////////////////////////////////////////////////////

CatalogImpl::CatalogImpl(AssetCatalog* catalog) //
  : _catalog(catalog) {  //
    // State is now initialized via LockedResource default constructor
    
    // Initialize root namespace
    _root_namespace = std::make_shared<AssetNamespace>("");
    _root_namespace->_full_path = "";
        
    // Initialize download manager with default concurrent queue
    _download_manager = std::make_shared<DownloadManager>(opq::ioQueue());
    
    // Initialize upload manager with default concurrent queue
    _upload_manager = std::make_shared<UploadManager>(opq::ioQueue());
  }
  
  ////////////////////////////////////////////////////////////////

assetlocation_ptr_t CatalogImpl::locateAsset(const assetid_t& fq_asset_id) const {
  assetlocation_ptr_t result;

  _state.atomicOp([&](const CatalogState& state) {
    auto it = state._entries_by_assetid.find(fq_asset_id);
    if (it != state._entries_by_assetid.end()) {
      result                   = std::make_shared<AssetLocation>();
      result->_namespace_id    = it->second->_namespace_id;
      result->_relative_path   = it->second->_asset_path;
      result->_source_manifest = it->second->_manifest;
      // All CDN content is encrypted (system invariant)
      result->_compression_type = it->second->_entry->_compression_type;
      result->_chunk_manifest   = it->second->_entry->_chunk_manifest;

      // Build location info from namespace configuration
      std::string namespace_id = it->second->_namespace_id;
      std::string remote_loc = "";
      if (_config_space) {
        remote_loc = _config_space->getNamespaceRemoteLocation(namespace_id);
      }
      std::string storage_hash = it->second->_entry->_storage_hash;

      if (!remote_loc.empty() && !storage_hash.empty()) {
        std::string base_url = remote_loc;

        // Check for environment variable pattern ${VAR_NAME}
        if (base_url.find("${") != std::string::npos) {
          // Find and expand all environment variables in the URL
          size_t pos = 0;
          while ((pos = base_url.find("${", pos)) != std::string::npos) {
            size_t end_pos = base_url.find("}", pos);
            if (end_pos != std::string::npos) {
              std::string var_name  = base_url.substr(pos + 2, end_pos - pos - 2);
              const char* env_value = std::getenv(var_name.c_str());
              if (env_value) {
                base_url.replace(pos, end_pos - pos + 1, env_value);
                pos += strlen(env_value);
              } else {
                logchan_catalog->log("WARNING: Environment variable %s not found in remote_loc", var_name.c_str());
                pos = end_pos + 1;
              }
            } else {
              break;
            }
          }
        }

        // Handle template format <location_key>
        if (base_url.find("<") == 0 && base_url.find(">") != std::string::npos) {
          // Extract the location key from template
          size_t end_pos           = base_url.find(">");
          std::string location_key = base_url.substr(1, end_pos - 1);

          // Get config for this namespace and resolve the location
          if (_config_space) {
            auto configs = _config_space->_configs;
            // Location key resolution logged at higher level if needed
            for (const auto& [config_id, config] : configs) {
              auto location_info = config->resolveRemoteLocation(location_key);
              if (location_info) {
                std::string resolved_url = location_info->_download_url.toString();

                // Check if URL was actually resolved (not still a template)
                if ((resolved_url.find("<") == 0 && resolved_url.find(">") != std::string::npos) || resolved_url == location_key) {
                  printf("[DEBUG] WARNING: URL is still a template: %s - continuing to next config\n", resolved_url.c_str());
                  continue; // Skip this config, try next one
                }

                // Location found logged at higher level if needed
                base_url               = resolved_url;
                result->_location_info = location_info; // Store the location_info
                break;
              } else {
                printf("[DEBUG] Config %s has no location for %s\n", config_id.c_str(), location_key.c_str());
              }
            }
          }
        }
        // Handle plain location key format (without brackets)
        else if (!base_url.empty() && base_url.find("http") != 0 && base_url.find("/") != 0) {
          // This looks like a location key (not a URL or path), try to resolve it
          std::string location_key = base_url;
          
          if (_config_space) {
            auto configs = _config_space->_configs;
            for (const auto& [config_id, config] : configs) {
              auto location_info = config->resolveRemoteLocation(location_key);
              if (location_info) {
                std::string resolved_url = location_info->_download_url.toString();
                
                // Check if URL was actually resolved
                if (!resolved_url.empty() && resolved_url != location_key) {
                  base_url               = resolved_url;
                  result->_location_info = location_info; // Store the location_info
                  break;
                }
              }
            }
          }
        }

        result->_base_url      = base_url;
        result->_relative_path = storage_hash + ".enc";
      } else if (!it->second->_entry->_local_loc.empty()) {
        // Fallback to local location if remote location is empty
        result->_base_url = it->second->_entry->_local_loc;
      }
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////

std::regex CatalogImpl::wildcardToRegex(const std::string& pattern) {
  std::string regex_str;
  for (char c : pattern) {
    switch (c) {
      case '*':
        regex_str += ".*";
        break;
      case '?':
        regex_str += ".";
        break;
      case '.':
        regex_str += "\\.";
        break;
      case '\\':
        regex_str += "\\\\";
        break;
      default:
        regex_str += c;
        break;
    }
  }
  return std::regex(regex_str);
}

  ////////////////////////////////////////////////////////////////

localmanifest_ptr_t CatalogImpl::_loadLocalManifest(const file::Path& manifest_path) {
  if(not manifest_path.doesPathExist()) return nullptr;
  localmanifest_ptr_t local_manifest;
  // Load manifest
  std::string manifest_data;
  FILE* fp = fopen(manifest_path.c_str(), "r");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    manifest_data.resize(size);
    fread(&manifest_data[0], 1, size, fp);
    fclose(fp);
  }
  
  auto manifest_json = nlohmann::json::parse(manifest_data);
  local_manifest = std::make_shared<LocalManifest>();
  // Check if we have the encrypted file locally
  local_manifest->_storage_hash = manifest_json["storage_hash"].get<std::string>();
  local_manifest->_encrypted_path = manifest_json["storage_hash"].get<std::string>();
  local_manifest->_content_hash = manifest_json["content_hash"].get<std::string>();
  local_manifest->_auto_unwrap = manifest_json["auto_unwrap"].get<bool>();
  local_manifest->_unwrapped_path = manifest_json["unwrapped_file"].get<std::string>();
  local_manifest->_fqid = manifest_json["fqid"].get<std::string>();
  local_manifest->_type = manifest_json["type"].get<std::string>();
  local_manifest->_archive_size = manifest_json["archive_size"].get<size_t>();
  local_manifest->_encrypted_size = manifest_json["encrypted_size"].get<size_t>();
  local_manifest->_compressed_size = manifest_json["compressed_size"].get<size_t>();
  local_manifest->_timestamp = manifest_json["timestamp"].get<std::string>();
  return local_manifest;
}

////////////////////////////////////////////////////////////////

void CatalogImpl::_saveLocalManifest(localmanifest_ptr_t mani, const file::Path& path){
  
  printf("[DEBUG] begin Saving local manifest to %s\n", path.c_str());
  OrkAssert(mani!=nullptr);

  printf("[DEBUG] mani->_storage_hash %s\n", mani->_storage_hash.c_str());
  printf("[DEBUG] mani->_content_hash %s\n", mani->_content_hash.c_str());
  printf("[DEBUG] mani->_unwrapped_path %s\n", mani->_unwrapped_path.c_str());
  printf("[DEBUG] mani->_type %s\n", mani->_type.c_str());
  printf("[DEBUG] mani->_timestamp %s\n", mani->_timestamp.c_str());

  nlohmann::json local_manifest;
  local_manifest["auto_unwrap"] = mani->_auto_unwrap;
  local_manifest["unwrapped_file"] = mani->_unwrapped_path;
  local_manifest["fqid"] = mani->_fqid;
  local_manifest["storage_hash"] = mani->_storage_hash;
  local_manifest["content_hash"] = mani->_content_hash;
  local_manifest["type"] = mani->_type;
  local_manifest["archive_size"] = mani->_archive_size;
  local_manifest["encrypted_size"] = mani->_encrypted_size;
  local_manifest["compressed_size"] = mani->_compressed_size;
  local_manifest["timestamp"] = mani->_timestamp;
  
  // Save local manifest
  file::Path local_manifest_dir = path.toAbsoluteFolder();
  local_manifest_dir.ensureDirectoryExists();
  
  FILE* fp = fopen(path.c_str(), "w");
  if (fp) {
    std::string manifest_str = local_manifest.dump(4);
    printf("[DEBUG] json encode: %s\n", manifest_str.c_str());
    fwrite(manifest_str.c_str(), 1, manifest_str.size(), fp);
    fclose(fp);
  } else {
    logchan_catalog->log("ERROR: Failed to save local manifest to %s", path.c_str());
    OrkAssert(false);
  }
  printf("[DEBUG] end Saving local manifest to %s\n", path.c_str());
}

////////////////////////////////////////////////////////////////
} // namespace ork::asset::catalog
