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
#include <ork/kernel/svariant.h>
#include <map>
#include <string>
#include <memory>
#include <functional>

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
    std::vector<std::string> _platforms; // Supported platforms: ["mac"], ["linux"], or ["mac", "linux"]
    
    // Check if this asset supports the current platform
    bool supportsCurrentPlatform() const;
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

struct AssetRequest;
using assetreq_ptr_t = std::shared_ptr<AssetRequest>;

////////////////////////////////////////////////////////////////////////////////

struct AssetRequest {
  
  //////////////////////////////////////////////////////////////////////////////
  // Configuration
  //////////////////////////////////////////////////////////////////////////////
  
  std::string _namespace;          // Catalog namespace to use
  std::string _asset_id;           // Specific asset ID (optional)
  
  //////////////////////////////////////////////////////////////////////////////
  // Callbacks
  //////////////////////////////////////////////////////////////////////////////
  
  using progress_fn_t = std::function<void(size_t downloaded, size_t total)>;
  ItemAndData<progress_fn_t> _progress_callback;
  
  //////////////////////////////////////////////////////////////////////////////
  // Methods
  //////////////////////////////////////////////////////////////////////////////
  
  AssetRequest();
  AssetRequest(const std::string& ns);
  AssetRequest(const std::string& ns, const std::string& asset_id);
  
  bool isValid() const;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog