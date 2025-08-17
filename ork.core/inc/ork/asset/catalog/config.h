////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/util/URL.h>
#include <ork/file/path.h>
#include <ork/asset/catalog/types.h>
#include <ork/kernel/mutex.h>
#include <map>
#include <memory>
#include <vector>

namespace ork::asset::catalog {

// Type aliases moved to types.h

////////////////////////////////////////////////////////////////////////////////

struct LocationInfo {
  URL _download_url;
  URL _upload_url;  
  std::optional<std::string> _api_key_read;   // API key for read operations (downloads)
  std::optional<std::string> _api_key_write;  // API key for write operations (uploads)
  bool _disable_cert_check = false;  // For self-signed certificates
  std::optional<std::string> _scp_destination;  // For SCP upload in format hostname:dest_dir
  
  // Get effective API keys (env var takes precedence)
  std::string getEffectiveReadApiKey(const std::string& location_name) const;
  std::string getEffectiveWriteApiKey(const std::string& location_name) const;
};

// locationinfo_ptr_t defined in types.h

////////////////////////////////////////////////////////////////////////////////

struct NamespaceConfig {
  std::string _encryption_key;        // Key for encrypting/decrypting assets in this namespace
  std::string _remote_location;       // Reference to location in remote_location_map_t
  
  NamespaceConfig() = default;
  NamespaceConfig(const std::string& key, const std::string& remote_loc) 
    : _encryption_key(key), _remote_location(remote_loc) {}
};

////////////////////////////////////////////////////////////////////////////////

struct AssetConfig {
  AssetConfig();
  ~AssetConfig();
  
  //////////////////////////////////////////////////////////////////////////////
  // Configuration Data
  //////////////////////////////////////////////////////////////////////////////
  namespaceconfig_map_t _namespaces;     // Namespace configurations with encryption keys and location bindings
  remote_location_map_t _remote_locations;       // Remote URLs with API keys
  local_location_map_t _local_locations;     // Local paths
  
  //////////////////////////////////////////////////////////////////////////////
  // Methods
  //////////////////////////////////////////////////////////////////////////////
  
  // Load and merge configs from directory
  static assetconfig_ptr_t loadFromDirectory(const file::Path& dir);
  
  // Load from single JSON file
  static assetconfig_ptr_t loadFromFile(const file::Path& file);
  
  // Merge another config into this one (other takes priority)
  void merge(const AssetConfig& other);
  
  // Resolve paths and URLs (no templates needed for content-addressable storage)
  file::Path resolveLocalPath(const std::string& path) const;
  locationinfo_ptr_t resolveRemoteLocation(const std::string& location_name) const;
  
  // Namespace configuration helpers
  std::string getEncryptionKeyForNamespace(const std::string& namespace_id) const;
  locationinfo_ptr_t getRemoteLocationForNamespace(const std::string& namespace_id) const;
  
  // Mutation methods for building configs programmatically
  void addNamespace(const std::string& namespace_id, const std::string& encryption_key, const std::string& remote_location);
  void addRemoteLocation(const std::string& id, const std::string& loc);
  void addLocalLocation(const std::string& id, const std::string& loc);
  
  // Serialization
  std::string toJson() const;
  
  // Implementation
  svar64_t _impl;
};

////////////////////////////////////////////////////////////////////////////////
// AssetConfigSpace - Container for multiple configurations
// Allows composing configs from multiple sources
////////////////////////////////////////////////////////////////////////////////

struct AssetConfigSpace {
  AssetConfigSpace();
  ~AssetConfigSpace();
  
  static assetconfigspace_ptr_t loadGlobalConfigs();
  static assetconfigspace_ptr_t loadFromDisk(const path_list_t& paths);
  static assetconfig_ptr_t loadConfigFromDisk(assetconfigspace_ptr_t space, const file::Path& path);

  // Factory method to create a config with ID
  assetconfig_ptr_t createConfig(const std::string& id, const file::Path& file);
  
  // Get config by ID
  assetconfig_ptr_t getConfig(const std::string& id) const;
  // Write all configs to disk
  void writeToDisk() const;
  
  // Get merged config (lazy-computed)
  assetconfig_ptr_t merged();
  
  // Mark merged config as dirty (for external modifications)
  void markDirty();
  
  // Get remote location for a namespace
  std::string getNamespaceRemoteLocation(const std::string& namespace_id) const;
  
  
  // Map of config ID to config object
  std::map<std::string, assetconfig_ptr_t> _configs;
  // Map of config ID to original file path
  std::map<std::string, file::Path> _config_paths;

private:
  // Lazy-computed merged config (nullptr means dirty)
  LockedResource<assetconfig_ptr_t> _merged;
  
  // Mark merged config as dirty
  void _markDirty();
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog