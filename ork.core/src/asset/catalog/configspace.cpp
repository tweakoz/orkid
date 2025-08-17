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

namespace ork::asset::catalog {
static logchannel_ptr_t logchan_cfgspc = logger()->configureChannel("CONFIGSPACE",fvec3(1,1,0),true);

////////////////////////////////////////////////////////////////////////////////
// AssetConfigSpace implementation
////////////////////////////////////////////////////////////////////////////////

AssetConfigSpace::AssetConfigSpace() {
  // Initialize _merged as dirty (nullptr)
  _merged.atomicOp([](assetconfig_ptr_t& merged) {
    merged = nullptr;
  });
}

////////////////////////////////////////////////////////////////////////////////

AssetConfigSpace::~AssetConfigSpace() {
}

  ////////////////////////////////////////////////////////////////////////////////

  assetconfigspace_ptr_t AssetConfigSpace::loadGlobalConfigs() {
    //logchan_cfgspc->log("Loading global asset configs into AssetConfigSpace");

    auto space = std::make_shared<AssetConfigSpace>();
    // Get ORKID_ASSET_MANIFEST_DIRS environment variable
    const char* manifest_dirs_env = getenv("ORKID_ASSET_MANIFEST_DIRS");
    if (!manifest_dirs_env || strlen(manifest_dirs_env) == 0) {
      logchan_cfgspc->log("ORKID_ASSET_MANIFEST_DIRS not set");
      return space;
    }
    //logchan_cfgspc->log("ORKID_ASSET_MANIFEST_DIRS: %s", manifest_dirs_env);    
    std::string manifest_dirs_str(manifest_dirs_env);
    std::vector<std::string> manifest_dirs;
    
    // Split by colon (Unix path separator)
    size_t start = 0;
    size_t end = manifest_dirs_str.find(':');
    while (end != std::string::npos) {
      std::string dir = manifest_dirs_str.substr(start, end - start);
      if (!dir.empty()) {
        manifest_dirs.push_back(dir);
      }
      start = end + 1;
      end = manifest_dirs_str.find(':', start);
    }
    // Add the last directory
    std::string last_dir = manifest_dirs_str.substr(start);
    if (!last_dir.empty()) {
      manifest_dirs.push_back(last_dir);
    }
    
    // Load config.json files from each directory
    for (const auto& manifest_dir : manifest_dirs) {
      file::Path config_path(manifest_dir);
      config_path = config_path / "config.json";
      
      if (config_path.doesPathExist()) {
        // Use the static method to load config into our space
        loadConfigFromDisk(space, config_path);
      }
    }
    
    return space;
  }

  ////////////////////////////////////////////////////////////////////////////////

assetconfig_ptr_t AssetConfigSpace::merged() {
  assetconfig_ptr_t result = nullptr;
  
  _merged.atomicOp([&](assetconfig_ptr_t& merged_config) {
    if (!merged_config) {
      // Dirty - recompute merged config
      merged_config = std::make_shared<AssetConfig>();
      
      
      // Merge all configs in order
      for (const auto& [id, config] : _configs) {
        if (config) {
          merged_config->merge(*config);
        }
      }

      // Add built-in destinations first
      merged_config->_local_locations["stage"] = file::Path::stage_dir();
      merged_config->_local_locations["assetcache"] = file::Path::stage_dir() / "assetcache";
      merged_config->_local_locations["shared"] = file::Path::stage_dir() / "share";

    }
    result = merged_config;
  });
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////

void AssetConfigSpace::markDirty() {
  _markDirty();
}

////////////////////////////////////////////////////////////////////////////////

void AssetConfigSpace::_markDirty() {
  _merged.atomicOp([](assetconfig_ptr_t& merged_config) {
    merged_config = nullptr;
  });
}

////////////////////////////////////////////////////////////////////////////////

assetconfig_ptr_t AssetConfigSpace::createConfig(const std::string& id, const file::Path& file) {
  // Create a new empty config
  auto config = std::make_shared<AssetConfig>();
  _configs[id] = config;
  _config_paths[id] = file;  // Track the original file path
  _markDirty();  // New config added, merged is now dirty
  return config;
}

////////////////////////////////////////////////////////////////////////////////

assetconfig_ptr_t AssetConfigSpace::getConfig(const std::string& id) const {
  auto it = _configs.find(id);
  if (it != _configs.end()) {
    return it->second;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

void AssetConfigSpace::writeToDisk() const {
  // Write each config back to its original file
  for (const auto& [id, config] : _configs) {
    if (config) {
      // Find the original file path
      auto path_it = _config_paths.find(id);
      if (path_it != _config_paths.end()) {
        const file::Path& file_path = path_it->second;
        std::string json_str = config->toJson();
        
        // Write to file using std::ofstream
        std::ofstream ofs(file_path.c_str());
        if (ofs.is_open()) {
          ofs << json_str;
          ofs.close();
          logchan_cfgspc->log("Wrote config '%s' to %s", 
                               id.c_str(), file_path.c_str());
        } else {
          logchan_cfgspc->log("ERROR: Could not open file %s for writing", 
                               file_path.c_str());
        }
      } else {
        logchan_cfgspc->log("WARNING: No file path tracked for config '%s'", 
                             id.c_str());
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

assetconfig_ptr_t AssetConfigSpace::loadConfigFromDisk(assetconfigspace_ptr_t space, const file::Path& path) {
  auto result = AssetConfig::loadFromFile(path);  

  // Generate ID from path filename
  std::string filename = path.toStdString();
  //logchan_cfgspc->log("space<%p> Loading config from: %s ID<%s>", (void*) space.get(), path.c_str(), filename.c_str());
  
  // Add to configs and mark dirty
  space->_configs[filename] = result;
  space->_config_paths[filename] = path;
  space->_markDirty();
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////

assetconfigspace_ptr_t AssetConfigSpace::loadFromDisk(const path_list_t& paths) {
  auto space = std::make_shared<AssetConfigSpace>();
  
  // For each path, create a config with ID based on filename
  for (const auto& path : paths) {
    std::string filename = path.toStdString();
    size_t last_slash = filename.find_last_of("/\\");
    size_t last_dot = filename.find_last_of(".");
    
    std::string id;
    if (last_slash != std::string::npos && last_dot != std::string::npos && last_dot > last_slash) {
      id = filename.substr(last_slash + 1, last_dot - last_slash - 1);
    } else if (last_dot != std::string::npos) {
      id = filename.substr(0, last_dot);
    } else {
      id = filename;
    }
    
    // createConfig already tracks the path
    space->createConfig(id, path);
  }
  
  return space;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::asset::catalog {
