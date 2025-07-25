////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/asset_config.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/string/string.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <fstream>
#include <sstream>
#include <glob.h>
#include <ork/application/application.h>
#include <algorithm>
#include <cctype>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////////////////////

std::string LocationInfo::getEffectiveApiKey(const std::string& location_name) const {
  // Transform location name to uppercase and replace non-alphanumeric with underscore
  std::string env_var_name = "ORKID_ASSET_API_KEY_";
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
  
  // Check for global fallback
  if (genviron.get("ORKID_ASSET_API_KEY_DEFAULT", env_value) && !env_value.empty()) {
    return env_value;
  }
  
  // Fall back to configured value
  return api_key.value_or("");
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
  config->processDestinationTemplates();
  
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
  config->parseFromJsonInternal(buffer.str());
  return config;
}

////////////////////////////////////////////////////////////////////////////////

void AssetConfig::merge(const AssetConfig& other) {
  // Merge namespace keys
  for (const auto& [key, value] : other._namespace_keys) {
    _namespace_keys[key] = value;
  }
  
  // Merge locations (deep copy LocationInfo)
  for (const auto& [key, value] : other._locations) {
    if (value) {
      auto new_loc = std::make_shared<LocationInfo>();
      new_loc->url = value->url;
      new_loc->api_key = value->api_key;
      new_loc->disable_cert_check = value->disable_cert_check;
      new_loc->scp_destination = value->scp_destination;
      _locations[key] = new_loc;
    }
  }
  
  // Merge destinations
  for (const auto& [key, value] : other._destinations) {
    _destinations[key] = value;
  }
}

////////////////////////////////////////////////////////////////////////////////

void AssetConfig::parseFromJsonInternal(const std::string& json_str) {
  rapidjson::Document doc;
  
  // Parse JSON
  doc.Parse(json_str.c_str());
  
  if (doc.HasParseError()) {
    auto error_code = doc.GetParseError();
    auto error_offset = doc.GetErrorOffset();
    printf("JSON parse error at offset %zu: %s\n", error_offset, rapidjson::GetParseError_En(error_code));
    return;
  }
  
  ///////////////////////////////////////////////////////////
  // Parse namespace_keys
  ///////////////////////////////////////////////////////////
  if (doc.HasMember("namespace_keys") && doc["namespace_keys"].IsObject()) {
    const auto& keys = doc["namespace_keys"];
    for (auto it = keys.MemberBegin(); it != keys.MemberEnd(); ++it) {
      if (it->value.IsString()) {
        _namespace_keys[it->name.GetString()] = it->value.GetString();
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
          if (url_str.substr(0, 4) == "http") {
            //printf("Parsing URL: %s\n", url_str.c_str());
            loc_info->url = URL(url_str);
          } else {
            loc_info->url = URL("file://" + url_str);
          }
        }
        
        if (it->value.HasMember("api_key") && it->value["api_key"].IsString()) {
          auto api_key_str = it->value["api_key"].GetString();
          //printf("Parsing API key: %s\n", api_key_str);
          loc_info->api_key = api_key_str;
        }
        
        if (it->value.HasMember("disable_cert_check") && it->value["disable_cert_check"].IsBool()) {
          loc_info->disable_cert_check = it->value["disable_cert_check"].GetBool();
        }
        
        if (it->value.HasMember("scp_destination") && it->value["scp_destination"].IsString()) {
          loc_info->scp_destination = it->value["scp_destination"].GetString();
        }
        
        _locations[it->name.GetString()] = loc_info;
      } else if (it->value.IsString()) {
        // Backward compatibility: string format
        std::string value = it->value.GetString();
        if (value.substr(0, 4) == "http") {
          loc_info->url = URL(value);
        } else {
          loc_info->url = URL("file://" + value);
        }
        _locations[it->name.GetString()] = loc_info;
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
        _destinations[it->name.GetString()] = file::Path(it->value.GetString());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void AssetConfig::processDestinationTemplates() {
  std::map<std::string, file::Path> processed;
  
  for (const auto& [key, value] : _destinations) {
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
  
  _destinations = processed;
}

////////////////////////////////////////////////////////////////////////////////

file::Path AssetConfig::resolvePath(const std::string& template_path) const {
  if (template_path.empty()) return file::Path();
  
  std::string path = template_path;
  
  // Replace all known destination templates
  for (const auto& [key, value] : _destinations) {
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

URL AssetConfig::resolveURL(const std::string& template_url) const {
  if (template_url.empty()) return URL();
  
  std::string url = template_url;
  
  // Replace all known location templates
  for (const auto& [key, value] : _locations) {
    if (value) {
      std::string token = "<" + key + ">";
      size_t pos = 0;
      while ((pos = url.find(token, pos)) != std::string::npos) {
        url.replace(pos, token.length(), value->url.toString());
        pos += value->url.toString().length();
      }
    }
  }
  
  return URL(url);
}

////////////////////////////////////////////////////////////////////////////////

locationinfo_ptr_t AssetConfig::resolveLocation(const std::string& template_url) const {
  if (template_url.empty()) return nullptr;
  
  // Check if the template starts with a known location key
  if (template_url.find("<") == 0) {
    size_t end_pos = template_url.find(">");
    if (end_pos != std::string::npos) {
      std::string location_key = template_url.substr(1, end_pos - 1);
      auto it = _locations.find(location_key);
      if (it != _locations.end()) {
        // Create a new LocationInfo with resolved URL
        auto resolved = std::make_shared<LocationInfo>();
        resolved->api_key = it->second->api_key;
        resolved->disable_cert_check = it->second->disable_cert_check;
        
        // If there's a path after the location key, append it
        if (end_pos + 1 < template_url.length()) {
          std::string path_suffix = template_url.substr(end_pos + 1);
          if (path_suffix[0] == '/') {
            resolved->url = it->second->url / path_suffix.substr(1);
          } else {
            resolved->url = it->second->url / path_suffix;
          }
        } else {
          resolved->url = it->second->url;
        }
        
        return resolved;
      }
    }
  }
  
  // Not a template, just return a LocationInfo with the URL
  auto result = std::make_shared<LocationInfo>();
  result->url = URL(template_url);
  return result;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog