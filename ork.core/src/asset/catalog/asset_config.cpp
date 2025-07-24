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
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <fstream>
#include <sstream>
#include <glob.h>
#include <ork/application/application.h>

namespace ork::asset::catalog {

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
  
  // Merge locations
  for (const auto& [key, value] : other._locations) {
    _locations[key] = value;
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
      if (it->value.IsString()) {
        std::string value = it->value.GetString();
        if (value.substr(0, 4) == "http") {
          _locations[it->name.GetString()] = URL(value);
        } else {
          // Non-URL location, store as file URL
          _locations[it->name.GetString()] = URL("file://" + value);
        }
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
      path_str = path_str.replace(0, 7, stage_path.c_str());  // 7 = len("<stage>")
      if (path_str[0] == '/') path_str = path_str.substr(1);  // Remove leading slash
      processed[key] = stage_path / path_str;
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
    std::string token = "<" + key + ">";
    size_t pos = 0;
    while ((pos = url.find(token, pos)) != std::string::npos) {
      url.replace(pos, token.length(), value.toString());
      pos += value.toString().length();
    }
  }
  
  return URL(url);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog