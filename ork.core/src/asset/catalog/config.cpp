////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/config.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/string/string.h>
#include <ork/util/logger.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <sstream>
#include <glob.h>
#include <ork/application/application.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////////////////////
// ConfigImpl - Pimpl implementation for AssetConfig
////////////////////////////////////////////////////////////////////////////////

struct ConfigImpl {
  AssetConfig* _config;
  
  ConfigImpl(AssetConfig* config) : _config(config) {}
  
  // Internal parsing
  void parseFromJsonInternal(const std::string& json_str);
  
  // Process destination paths
  void processDestinationPaths();
};

////////////////////////////////////////////////////////////////////////////////
// AssetConfig constructors
////////////////////////////////////////////////////////////////////////////////

AssetConfig::AssetConfig() {
  auto impl = _impl.makeShared<ConfigImpl>(this);
}

AssetConfig::~AssetConfig() {
}

////////////////////////////////////////////////////////////////////////////////

std::string LocationInfo::getEffectiveReadApiKey(const std::string& location_name) const {
  // Transform location name to uppercase and replace non-alphanumeric with underscore
  std::string env_var_name = "ORKID_ASSET_API_KEY_READ_";
  for (char c : location_name) {
    if (std::isalnum(c)) {
      env_var_name += std::toupper(c);
    } else {
      env_var_name += '_';
    }
  }
  
  // Check environment variable first
  std::string env_value;
  if (genviron.get(env_var_name, env_value) && !env_value.empty()) {
    return env_value;
  }
  
  // Fall back to configured value
  return _api_key_read.value_or("");
}

std::string LocationInfo::getEffectiveWriteApiKey(const std::string& location_name) const {
  // Transform location name to uppercase and replace non-alphanumeric with underscore
  std::string env_var_name = "ORKID_ASSET_API_KEY_WRITE_";
  for (char c : location_name) {
    if (std::isalnum(c)) {
      env_var_name += std::toupper(c);
    } else {
      env_var_name += '_';
    }
  }
  
  // Check environment variable first
  std::string env_value;
  if (genviron.get(env_var_name, env_value) && !env_value.empty()) {
    return env_value;
  }
  
  // Fall back to configured value
  return _api_key_write.value_or("");
}

////////////////////////////////////////////////////////////////////////////////

assetconfig_ptr_t AssetConfig::loadFromDirectory(const file::Path& dir) {
  auto config = std::make_shared<AssetConfig>();
  
  // Find all config files in directory
  std::string pattern = dir.c_str();
  pattern += "/*config.json";
  
  glob_t glob_result;
  if (glob(pattern.c_str(), GLOB_TILDE, nullptr, &glob_result) == 0) {
    for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
      file::Path config_file(glob_result.gl_pathv[i]);
      auto partial_config = loadFromFile(config_file);
      if (partial_config) {
        config->merge(*partial_config);
      }
    }
    globfree(&glob_result);
  }
  
  // Process templates after all configs are merged
  auto impl = config->_impl.getShared<ConfigImpl>();
  impl->processDestinationPaths();
  
  return config;
}

////////////////////////////////////////////////////////////////////////////////

assetconfig_ptr_t AssetConfig::loadFromFile(const file::Path& file) {
  // Read file contents
  std::ifstream fs(file.c_str());
  if (!fs.is_open()) {
    return nullptr;
  }
  
  std::stringstream buffer;
  buffer << fs.rdbuf();
  fs.close();
  
  auto config = std::make_shared<AssetConfig>();
  auto impl = config->_impl.getShared<ConfigImpl>();
  impl->parseFromJsonInternal(buffer.str());
  return config;
}

////////////////////////////////////////////////////////////////////////////////

void AssetConfig::merge(const AssetConfig& other) {

  auto this_json = this->toJson();
  auto other_json = other.toJson();
  //printf("Merging AssetConfig:\nThis: %s\nOther: %s\n", this_json.c_str(), other_json.c_str());
  // Merge namespaces
  for (const auto& [key, value] : other._namespaces) {
    auto it = _namespaces.find(key);
    if( it != _namespaces.end() ) {
      logchan_catalog->log("ERROR: Overwriting existing namespace '%s'", key.c_str());
      OrkAssert(false);
    }
    _namespaces[key] = value;
  }
  
  // Merge locations (deep copy LocationInfo)
  for (const auto& [key, value] : other._remote_locations) {
    if (value) {
      auto it = _remote_locations.find(key);
      if( it != _remote_locations.end()) {
        logchan_catalog->log("ERROR: Overwriting existing remote location '%s'", key.c_str());
        OrkAssert(false);
      }
      auto new_loc = std::make_shared<LocationInfo>();
      new_loc->_download_url = value->_download_url;
      new_loc->_upload_url = value->_upload_url;
      new_loc->_api_key_read = value->_api_key_read;
      new_loc->_api_key_write = value->_api_key_write;
      new_loc->_disable_cert_check = value->_disable_cert_check;
      new_loc->_scp_destination = value->_scp_destination;
      _remote_locations[key] = new_loc;
    }
  }
  
  // Merge destinations
  for (const auto& [key, value] : other._local_locations) {
    auto it = _local_locations.find(key);
    if( it != _local_locations.end() ) {
      logchan_catalog->log("ERROR: Overwriting existing local location '%s'", key.c_str());
      OrkAssert(false);
    }
    _local_locations[key] = value;
  }
  this_json = this->toJson();
  //printf("Merged After: %s\n", this_json.c_str());
}

////////////////////////////////////////////////////////////////////////////////

void ConfigImpl::parseFromJsonInternal(const std::string& json_str) {
  rapidjson::Document doc;
  
  // JSON parsing logged at higher level if needed
  // Parse JSON
  doc.Parse(json_str.c_str());
  
  if (doc.HasParseError()) {
    auto error_code = doc.GetParseError();
    auto error_offset = doc.GetErrorOffset();
    logchan_catalog->log("ERROR: JSON parse error at offset %zu: %s", error_offset, rapidjson::GetParseError_En(error_code));
    return;
  }
  
  ///////////////////////////////////////////////////////////
  // Parse namespaces
  ///////////////////////////////////////////////////////////
  if (doc.HasMember("namespaces") && doc["namespaces"].IsObject()) {
    const auto& namespaces = doc["namespaces"];
    for (auto it = namespaces.MemberBegin(); it != namespaces.MemberEnd(); ++it) {
      if (it->value.IsObject()) {
        const auto& ns_obj = it->value;
        auto ns_config = std::make_shared<NamespaceConfig>();
        
        // Parse encryption_key
        if (ns_obj.HasMember("encryption_key") && ns_obj["encryption_key"].IsString()) {
          std::string key_value = ns_obj["encryption_key"].GetString();
          
          // Check for environment variable pattern ${VAR_NAME}
          if (key_value.size() > 3 && key_value[0] == '$' && key_value[1] == '{' && key_value.back() == '}') {
            std::string var_name = key_value.substr(2, key_value.size() - 3);
            const char* env_value = std::getenv(var_name.c_str());
            if (env_value) {
              ns_config->_encryption_key = env_value;
            } else {
              logchan_catalog->log("WARNING: Environment variable %s not found for encryption_key", var_name.c_str());
              ns_config->_encryption_key = ""; // Clear value if env var not found
            }
          } else {
            ns_config->_encryption_key = key_value;
          }
        }
        
        // Parse remote_location
        if (ns_obj.HasMember("remote_location") && ns_obj["remote_location"].IsString()) {
          ns_config->_remote_location = ns_obj["remote_location"].GetString();
        }
        
        // Namespace config addition logged at higher level if needed
        
        _config->_namespaces[it->name.GetString()] = ns_config;
      }
    }
  }
  
  ///////////////////////////////////////////////////////////
  // Parse locations
  ///////////////////////////////////////////////////////////
  if (doc.HasMember("locations") && doc["locations"].IsObject()) {
    const auto& locs = doc["locations"];
    for (auto it = locs.MemberBegin(); it != locs.MemberEnd(); ++it) {
      auto loc_info = std::make_shared<LocationInfo>();
      
      if (it->value.IsObject()) {
        // New format: {"url": "...", "api_key": "..."}
        if (it->value.HasMember("url") && it->value["url"].IsString()) {
          std::string url_str = it->value["url"].GetString();
          
          // Check for environment variable pattern ${VAR_NAME} - expand all occurrences
          size_t pos = 0;
          while ((pos = url_str.find("${", pos)) != std::string::npos) {
            size_t end_pos = url_str.find("}", pos);
            if (end_pos != std::string::npos) {
              std::string var_name = url_str.substr(pos + 2, end_pos - pos - 2);
              const char* env_value = std::getenv(var_name.c_str());
              if (env_value) {
                url_str.replace(pos, end_pos - pos + 1, env_value);
                pos += strlen(env_value);
              } else {
                logchan_catalog->log("WARNING: Environment variable %s not found for url", var_name.c_str());
                pos = end_pos + 1;
              }
            } else {
              break;
            }
          }
          
          if (!url_str.empty()) {
            if (url_str.substr(0, 4) == "http") {
              loc_info->_download_url = URL(url_str);
            } else {
              loc_info->_download_url = URL("file://" + url_str);
            }
          }
        }
        
        // Parse upload_url (optional)
        if (it->value.HasMember("upload_url") && it->value["upload_url"].IsString()) {
          std::string upload_url_str = it->value["upload_url"].GetString();
          
          // Check for environment variable pattern ${VAR_NAME} - expand all occurrences
          size_t pos = 0;
          while ((pos = upload_url_str.find("${", pos)) != std::string::npos) {
            size_t end_pos = upload_url_str.find("}", pos);
            if (end_pos != std::string::npos) {
              std::string var_name = upload_url_str.substr(pos + 2, end_pos - pos - 2);
              const char* env_value = std::getenv(var_name.c_str());
              if (env_value) {
                upload_url_str.replace(pos, end_pos - pos + 1, env_value);
                pos += strlen(env_value);
              } else {
                logchan_catalog->log("WARNING: Environment variable %s not found for upload_url", var_name.c_str());
                pos = end_pos + 1;
              }
            } else {
              break;
            }
          }
          
          if (!upload_url_str.empty()) {
            if (upload_url_str.substr(0, 4) == "http") {
              loc_info->_upload_url = URL(upload_url_str);
            } else {
              loc_info->_upload_url = URL("file://" + upload_url_str);
            }
          }
        }
        
        // Parse api_key_read
        if (it->value.HasMember("api_key_read") && it->value["api_key_read"].IsString()) {
          std::string api_key_str = it->value["api_key_read"].GetString();
          
          // Check for environment variable pattern ${VAR_NAME}
          if (api_key_str.size() > 3 && api_key_str[0] == '$' && api_key_str[1] == '{' && api_key_str.back() == '}') {
            std::string var_name = api_key_str.substr(2, api_key_str.size() - 3);
            const char* env_value = std::getenv(var_name.c_str());
            if (env_value) {
              loc_info->_api_key_read = env_value;
            } else {
              logchan_catalog->log("WARNING: Environment variable %s not found for api_key_read", var_name.c_str());
              loc_info->_api_key_read = ""; // Clear value if env var not found
            }
          } else {
            loc_info->_api_key_read = api_key_str;
          }
        }
        
        // Parse api_key_write  
        if (it->value.HasMember("api_key_write") && it->value["api_key_write"].IsString()) {
          std::string api_key_str = it->value["api_key_write"].GetString();
          
          // Check for environment variable pattern ${VAR_NAME}
          if (api_key_str.size() > 3 && api_key_str[0] == '$' && api_key_str[1] == '{' && api_key_str.back() == '}') {
            std::string var_name = api_key_str.substr(2, api_key_str.size() - 3);
            const char* env_value = std::getenv(var_name.c_str());
            if (env_value) {
              loc_info->_api_key_write = env_value;
            } else {
              logchan_catalog->log("WARNING: Environment variable %s not found for api_key_write", var_name.c_str());
              loc_info->_api_key_write = ""; // Clear value if env var not found
            }
          } else {
            loc_info->_api_key_write = api_key_str;
          }
        }
        
        if (it->value.HasMember("disable_cert_check") && it->value["disable_cert_check"].IsBool()) {
          loc_info->_disable_cert_check = it->value["disable_cert_check"].GetBool();
        }
        
        if (it->value.HasMember("scp_destination") && it->value["scp_destination"].IsString()) {
          loc_info->_scp_destination = it->value["scp_destination"].GetString();
        }
        
        _config->_remote_locations[it->name.GetString()] = loc_info;
      } else if (it->value.IsString()) {
        // Backward compatibility: string format
        std::string value = it->value.GetString();
        if (value.substr(0, 4) == "http") {
          loc_info->_download_url = URL(value);
        } else {
          loc_info->_download_url = URL("file://" + value);
        }
        _config->_remote_locations[it->name.GetString()] = loc_info;
      }
    }
  }
  
  ///////////////////////////////////////////////////////////
  // Parse destinations (don't process templates yet)
  ///////////////////////////////////////////////////////////
  if (doc.HasMember("destinations") && doc["destinations"].IsObject()) {
    const auto& dests = doc["destinations"];
    for (auto it = dests.MemberBegin(); it != dests.MemberEnd(); ++it) {
      if (it->value.IsString()) {
        // Store as-is for now, will process templates later
        _config->_local_locations[it->name.GetString()] = file::Path(it->value.GetString());
      }
    }
  }

  //logchan_catalog->log("Parsed config<%p> from json: %s", (void*) _config, json_str.c_str());

}

////////////////////////////////////////////////////////////////////////////////

void ConfigImpl::processDestinationPaths() {
  local_location_map_t processed;
  
  for (const auto& [key, value] : _config->_local_locations) {
    std::string path_str = value.c_str();
    
    if (path_str.find("<stage>") == 0) {
      // Replace <stage> with actual stage path
      file::Path stage_path = file::Path::stage_dir();
      if (path_str == "<stage>") {
        // If it's exactly <stage>, just use the stage path
        processed[key] = stage_path;
      } else {
        // Otherwise, append the rest after <stage>/
        path_str = path_str.substr(7);  // Remove "<stage>"
        if (path_str[0] == '/') path_str = path_str.substr(1);  // Remove leading slash
        processed[key] = stage_path / path_str;
      }
    }
    else if (path_str.find("<temp>") == 0) {
      // Replace <temp> with actual temp path
      file::Path temp_path = file::Path::temp_dir();
      path_str = path_str.replace(0, 6, temp_path.c_str());  // 6 = len("<temp>")
      if (path_str[0] == '/') path_str = path_str.substr(1);  // Remove leading slash
      processed[key] = temp_path / path_str;
    }
    else {
      processed[key] = value;
    }
  }
  
  _config->_local_locations = processed;
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

file::Path AssetConfig::resolveLocalPath(const std::string& template_path) const {
  if (template_path.empty()) return file::Path();
  
  std::string path = template_path;
  
  // Replace all known destination templates
  for (const auto& [key, value] : _local_locations) {
    std::string token = "<" + key + ">";
    size_t pos = 0;
    while ((pos = path.find(token, pos)) != std::string::npos) {
      path.replace(pos, token.length(), value.c_str());
      pos += std::string(value.c_str()).length();
    }
  }
  
  return file::Path(path);
}

////////////////////////////////////////////////////////////////////////////////

locationinfo_ptr_t AssetConfig::resolveRemoteLocation(const std::string& location_ref) const {
  if (location_ref.empty()) return nullptr;
  
  // First check if it's a direct location key (no template brackets)
  auto it = _remote_locations.find(location_ref);
  if (it != _remote_locations.end()) {
    return it->second;  // Return the LocationInfo directly
  }
  
  // Check if the template starts with a known location key
  if (location_ref.find("<") == 0) {
    size_t end_pos = location_ref.find(">");
    if (end_pos != std::string::npos) {
      std::string location_key = location_ref.substr(1, end_pos - 1);
      auto template_it = _remote_locations.find(location_key);
      if (template_it != _remote_locations.end()) {
        // Create a new LocationInfo with resolved URL
        auto resolved = std::make_shared<LocationInfo>();
        resolved->_api_key_read = template_it->second->_api_key_read;
        resolved->_api_key_write = template_it->second->_api_key_write;
        resolved->_disable_cert_check = template_it->second->_disable_cert_check;
        resolved->_scp_destination = template_it->second->_scp_destination;  // Copy scp_destination too
        
        // If there's a path after the location key, append it
        if (end_pos + 1 < location_ref.length()) {
          std::string path_suffix = location_ref.substr(end_pos + 1);
          if (path_suffix[0] == '/') {
            resolved->_download_url = template_it->second->_download_url / path_suffix.substr(1);
            resolved->_upload_url = template_it->second->_upload_url / path_suffix.substr(1);
          } else {
            resolved->_download_url = template_it->second->_download_url / path_suffix;
          resolved->_upload_url = template_it->second->_upload_url / path_suffix;
          }
        } else {
          resolved->_download_url = template_it->second->_download_url;
          resolved->_upload_url = template_it->second->_upload_url; 
          
        }
        
        return resolved;
      }
    }
  }
  
  // Not a template, just return a LocationInfo with the URL
  auto result = std::make_shared<LocationInfo>();
  result->_download_url = URL(location_ref);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Namespace configuration helpers
////////////////////////////////////////////////////////////////////////////////

std::string AssetConfig::getEncryptionKeyForNamespace(const std::string& namespace_id) const {
  auto ns_it = _namespaces.find(namespace_id);
  if (ns_it != _namespaces.end()) {
    return ns_it->second->_encryption_key;
  }
  
  return ""; // No key found
}

locationinfo_ptr_t AssetConfig::getRemoteLocationForNamespace(const std::string& namespace_id) const {
  // Check _namespaces map for remote location reference
  auto ns_it = _namespaces.find(namespace_id);
  if (ns_it != _namespaces.end() && !ns_it->second->_remote_location.empty()) {
    return resolveRemoteLocation(ns_it->second->_remote_location);
  }
  
  return nullptr; // No remote location configured for this namespace
}

////////////////////////////////////////////////////////////////////////////////
// AssetConfig mutation methods
////////////////////////////////////////////////////////////////////////////////


void AssetConfig::addNamespace(const std::string& namespace_id, const std::string& encryption_key, const std::string& remote_location) {
  auto ns_config = std::make_shared<NamespaceConfig>(encryption_key, remote_location);
  _namespaces[namespace_id] = ns_config;
}

void AssetConfig::addRemoteLocation(const std::string& id, const std::string& loc) {
  auto location_info = std::make_shared<LocationInfo>();
  location_info->_download_url = URL(loc);
  _remote_locations[id] = location_info;
}

void AssetConfig::addLocalLocation(const std::string& id, const std::string& loc) {
  _local_locations[id] = file::Path(loc);
}

std::string AssetConfig::toJson() const {
  rapidjson::Document doc;
  doc.SetObject();
  auto& allocator = doc.GetAllocator();
  
  // Add namespaces
  rapidjson::Value namespaces(rapidjson::kObjectType);
  for (const auto& [key, ns_config] : _namespaces) {
    rapidjson::Value ns_obj(rapidjson::kObjectType);
    
    ns_obj.AddMember("encryption_key", 
                     rapidjson::Value(ns_config->_encryption_key.c_str(), allocator), 
                     allocator);
    ns_obj.AddMember("remote_location", 
                     rapidjson::Value(ns_config->_remote_location.c_str(), allocator), 
                     allocator);
    
    namespaces.AddMember(rapidjson::Value(key.c_str(), allocator), ns_obj, allocator);
  }
  doc.AddMember("namespaces", namespaces, allocator);
  
  // Add locations
  rapidjson::Value locations(rapidjson::kObjectType);
  for (const auto& [key, value] : _remote_locations) {
    if (value) {
      locations.AddMember(
        rapidjson::Value(key.c_str(), allocator),
        rapidjson::Value(value->_download_url.toString().c_str(), allocator),
        allocator
      );
    }
  }
  doc.AddMember("locations", locations, allocator);
  
  // Add destinations
  rapidjson::Value destinations(rapidjson::kObjectType);
  for (const auto& [key, value] : _local_locations) {
    destinations.AddMember(
      rapidjson::Value(key.c_str(), allocator),
      rapidjson::Value(value.c_str(), allocator),
      allocator
    );
  }
  doc.AddMember("destinations", destinations, allocator);
  
  // Convert to string
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  doc.Accept(writer);
  
  return buffer.GetString();
}

////////////////////////////////////////////////////////////////////////////////

std::string AssetConfigSpace::getNamespaceRemoteLocation(const std::string& namespace_id) const {
  // Search through all configs to find the namespace
  for (const auto& [config_id, config] : _configs) {
    auto it = config->_namespaces.find(namespace_id);
    if (it != config->_namespaces.end()) {
      return it->second->_remote_location;
    }
  }
  return ""; // Namespace not found
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog