////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/file/path.h>
#include <map>
#include <string>
#include <memory>

namespace ork::asset::catalog {

struct AssetManifest;
using assetmanifest_ptr_t = std::shared_ptr<AssetManifest>;

////////////////////////////////////////////////////////////////////////////////

struct AssetManifest {
  
  //////////////////////////////////////////////////////////////////////////////
  // Asset Entry
  //////////////////////////////////////////////////////////////////////////////
  struct AssetEntry {
    std::string _type;              // "asset_pak" or "asset"
    int _priority = 100;
    bool _merge = false;
    std::string _dst_loc;           // Destination location (with templates)
    std::string _src_loc;           // Source location (with templates)
    std::string _filename;
    std::string _md5;
    std::map<std::string, std::string> _dependencies;
    std::string _namespace;
    std::string _manifest_source;
  };
  
  //////////////////////////////////////////////////////////////////////////////
  // Manifest Data
  //////////////////////////////////////////////////////////////////////////////
  std::string _namespace;
  std::string _version;
  std::map<std::string, AssetEntry> _assets;
  
  //////////////////////////////////////////////////////////////////////////////
  // Methods
  //////////////////////////////////////////////////////////////////////////////
  
  // Load from JSON file
  static assetmanifest_ptr_t loadFromFile(const file::Path& path);
  
  // Parse from JSON string
  static assetmanifest_ptr_t parseFromString(const std::string& json_str, const file::Path& source_file);
  
private:
  // Internal parsing
  void parseFromJsonInternal(const std::string& json_str, const file::Path& source_file);
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog