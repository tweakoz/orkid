////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/asset_manifest.h>
#include <ork/kernel/string/deco.inl>
#include <ork/file/file.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <fstream>
#include <sstream>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////////////////////

assetmanifest_ptr_t AssetManifest::loadFromFile(const file::Path& path) {
  // Read file contents
  std::ifstream file(path.c_str());
  if (!file.is_open()) {
    return nullptr;
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();
  
  return parseFromString(buffer.str(), path);
}

////////////////////////////////////////////////////////////////////////////////

assetmanifest_ptr_t AssetManifest::parseFromString(const std::string& json_str, const file::Path& source_file) {
  auto manifest = std::make_shared<AssetManifest>();
  manifest->parseFromJsonInternal(json_str, source_file);
  return manifest;
}

////////////////////////////////////////////////////////////////////////////////

void AssetManifest::parseFromJsonInternal(const std::string& json_str, const file::Path& source_file) {
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
  // Check for new format (has namespace field)
  ///////////////////////////////////////////////////////////
  if (doc.HasMember("namespace") && doc["namespace"].IsString()) {
    // New format
    _namespace = doc["namespace"].GetString();
    
    if (doc.HasMember("version") && doc["version"].IsString()) {
      _version = doc["version"].GetString();
    }
    
    if (doc.HasMember("assets") && doc["assets"].IsObject()) {
      const auto& assets = doc["assets"];
      
      for (auto it = assets.MemberBegin(); it != assets.MemberEnd(); ++it) {
        std::string asset_id = it->name.GetString();
        const auto& asset_data = it->value;
        
        if (!asset_data.IsObject()) continue;
        
        AssetEntry entry;
        entry._namespace = _namespace;
        entry._manifest_source = source_file.c_str();
        
        ///////////////////////////////////////////////////////////
        // Parse asset fields
        ///////////////////////////////////////////////////////////
        if (asset_data.HasMember("type") && asset_data["type"].IsString()) {
          entry._type = asset_data["type"].GetString();
        }
        
        if (asset_data.HasMember("priority") && asset_data["priority"].IsInt()) {
          entry._priority = asset_data["priority"].GetInt();
        }
        
        if (asset_data.HasMember("merge") && asset_data["merge"].IsBool()) {
          entry._merge = asset_data["merge"].GetBool();
        }
        
        if (asset_data.HasMember("dst_loc") && asset_data["dst_loc"].IsString()) {
          entry._dst_loc = asset_data["dst_loc"].GetString();
        }
        
        if (asset_data.HasMember("src_loc") && asset_data["src_loc"].IsString()) {
          entry._src_loc = asset_data["src_loc"].GetString();
        }
        
        if (asset_data.HasMember("filename") && asset_data["filename"].IsString()) {
          entry._filename = asset_data["filename"].GetString();
        }
        
        if (asset_data.HasMember("md5") && asset_data["md5"].IsString()) {
          entry._md5 = asset_data["md5"].GetString();
        }
        
        ///////////////////////////////////////////////////////////
        // Parse dependencies
        ///////////////////////////////////////////////////////////
        if (asset_data.HasMember("dependencies") && asset_data["dependencies"].IsObject()) {
          const auto& deps = asset_data["dependencies"];
          for (auto dep_it = deps.MemberBegin(); dep_it != deps.MemberEnd(); ++dep_it) {
            if (dep_it->value.IsString()) {
              entry._dependencies[dep_it->name.GetString()] = dep_it->value.GetString();
            }
          }
        }
        
        _assets[asset_id] = entry;
      }
    }
  } else {
    ///////////////////////////////////////////////////////////
    // Old format - assume singularity namespace
    ///////////////////////////////////////////////////////////
    _namespace = "singularity";
    _version = "1.0.0";
    
    // Each top-level key is an asset
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
      std::string asset_id = it->name.GetString();
      const auto& asset_data = it->value;
      
      if (!asset_data.IsObject()) continue;
      
      AssetEntry entry;
      entry._namespace = _namespace;
      entry._manifest_source = source_file.c_str();
      entry._priority = 100;  // Default priority
      
      ///////////////////////////////////////////////////////////
      // Convert old field names
      ///////////////////////////////////////////////////////////
      if (asset_data.HasMember("type") && asset_data["type"].IsString()) {
        entry._type = asset_data["type"].GetString();
      }
      
      // Handle old "dest_path" -> "dst_loc"
      if (asset_data.HasMember("dest_path") && asset_data["dest_path"].IsString()) {
        entry._dst_loc = asset_data["dest_path"].GetString();
      } else if (asset_data.HasMember("dst_loc") && asset_data["dst_loc"].IsString()) {
        entry._dst_loc = asset_data["dst_loc"].GetString();
      }
      
      // Handle old "loc" -> "src_loc"
      if (asset_data.HasMember("loc") && asset_data["loc"].IsString()) {
        entry._src_loc = asset_data["loc"].GetString();
      } else if (asset_data.HasMember("src_loc") && asset_data["src_loc"].IsString()) {
        entry._src_loc = asset_data["src_loc"].GetString();
      }
      
      if (asset_data.HasMember("filename") && asset_data["filename"].IsString()) {
        entry._filename = asset_data["filename"].GetString();
      }
      
      if (asset_data.HasMember("md5") && asset_data["md5"].IsString()) {
        entry._md5 = asset_data["md5"].GetString();
      }
      
      _assets[asset_id] = entry;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

AssetRequest::AssetRequest() {
}

AssetRequest::AssetRequest(const std::string& ns) 
  : _namespace(ns) {
}

AssetRequest::AssetRequest(const std::string& ns, const std::string& asset_id)
  : _namespace(ns)
  , _asset_id(asset_id) {
}

bool AssetRequest::isValid() const {
  return !_namespace.empty();
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog