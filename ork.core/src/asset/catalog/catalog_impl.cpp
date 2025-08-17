#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
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

////////////////////////////////////////////////////////////////
// Internal Methods
////////////////////////////////////////////////////////////////

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->configureChannel("CATALOG", fvec3(1, 1, 0), true);

////////////////////////////////////////////////////////////////

assetlocation_ptr_t CatalogImpl::locateAsset(const assetid_t& fq_asset_id) const {
  assetlocation_ptr_t result;

  _state.atomicOp([&](const CatalogState& state) {
    auto it = state._entries_by_assetid.find(fq_asset_id);
    if (it != state._entries_by_assetid.end()) {
      result                   = std::make_shared<AssetLocation>();
      result->_namespace_id    = it->second.namespace_id;
      result->_relative_path   = it->second.asset_path;
      result->_source_manifest = it->second.manifest;
      // All CDN content is encrypted (system invariant)
      result->_is_encrypted     = true;
      result->_is_compressed    = it->second.entry->_is_compressed;
      result->_compression_type = it->second.entry->_compression_type;
      result->_chunk_manifest   = it->second.entry->_chunk_manifest;

      // Build location info from namespace configuration
      std::string namespace_id = it->second.namespace_id;
      std::string remote_loc = "";
      if (_config_space) {
        remote_loc = _config_space->getNamespaceRemoteLocation(namespace_id);
      }
      std::string storage_hash = it->second.entry->_storage_hash;

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

        result->_base_url      = base_url;
        result->_relative_path = storage_hash + ".enc";
      } else if (!it->second.entry->_local_loc.empty()) {
        // Fallback to local location if remote location is empty
        result->_base_url = it->second.entry->_local_loc;
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
} // namespace ork::asset::catalog
