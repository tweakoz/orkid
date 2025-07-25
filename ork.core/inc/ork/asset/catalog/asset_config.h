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
#include <map>
#include <memory>
#include <vector>

namespace ork::asset::catalog {

struct AssetConfig;
using assetconfig_ptr_t = std::shared_ptr<AssetConfig>;

////////////////////////////////////////////////////////////////////////////////

struct LocationInfo {
  URL url;
  std::optional<std::string> api_key;
  bool disable_cert_check = false;  // For self-signed certificates
  std::optional<std::string> scp_destination;  // For SCP upload in format hostname:dest_dir
  
  // Get effective API key (env var takes precedence)
  std::string getEffectiveApiKey(const std::string& location_name) const;
};

using locationinfo_ptr_t = std::shared_ptr<LocationInfo>;

////////////////////////////////////////////////////////////////////////////////

struct AssetConfig {
  
  //////////////////////////////////////////////////////////////////////////////
  // Configuration Data
  //////////////////////////////////////////////////////////////////////////////
  std::map<std::string, std::string> _namespace_keys;    // Decryption keys
  std::map<std::string, locationinfo_ptr_t> _locations;  // Remote URLs with API keys
  std::map<std::string, file::Path> _destinations;       // Local paths
  
  //////////////////////////////////////////////////////////////////////////////
  // Methods
  //////////////////////////////////////////////////////////////////////////////
  
  // Load and merge configs from directory
  static assetconfig_ptr_t loadFromDirectory(const file::Path& dir);
  
  // Load from single JSON file
  static assetconfig_ptr_t loadFromFile(const file::Path& file);
  
  // Merge another config into this one (other takes priority)
  void merge(const AssetConfig& other);
  
  // Resolve template strings
  file::Path resolvePath(const std::string& template_path) const;
  URL resolveURL(const std::string& template_url) const;
  locationinfo_ptr_t resolveLocation(const std::string& template_url) const;
  
private:
  // Internal parsing
  void parseFromJsonInternal(const std::string& json_str);
  
  // Process destination templates (e.g., <stage> -> actual path)
  void processDestinationTemplates();
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog