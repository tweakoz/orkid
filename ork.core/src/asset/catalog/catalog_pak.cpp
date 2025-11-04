////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#if !defined(ORK_IOS)

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/request.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <ork/util/logger.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include "catalog_impl.h"

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////////////////////

datablock_ptr_t AssetCatalog::_packFromLocal(assetfqid_ptr_t fqid) {

  auto source_dir = fqid->_source_dir;
  auto pak_local_path = fqid->_pak_local_path;

  Timer timer;
  timer.Start();

  auto asset_info = fqid->_asset_info;
  OrkAssert(asset_info);

  logchan_catalog->log("packFromLocal(direct): local_loc=%s", 
                       asset_info->_local_loc.c_str());

  // 2. Verify it's an asset_pak
  if (asset_info->_type != "asset_pak") {
    logchan_catalog->log("packFromLocal: Asset ID is not of type 'asset_pak': %s (type=%s)", 
                         asset_info->_id.c_str(), asset_info->_type.c_str());
    return nullptr;
  }

  logchan_catalog->log("packFromLocal: Looking for directory: %s (pak_local_path=%s, tar_root=%s)", 
                       source_dir.c_str(), pak_local_path.c_str(), asset_info->_tar_root.c_str());
  
  if (!source_dir.doesPathExist()) {
    logchan_catalog->log("packFromLocal: Source directory does not exist: %s", source_dir.c_str());
    return nullptr;
  }

  // 6. Create TAR from directory contents
  util::TarCreateOptions create_options;
  create_options.base_path = source_dir.c_str();
  
  // Apply filters if specified
  if (!asset_info->_filters.empty()) {
    logchan_catalog->log("Applying %zu filters to TAR creation", asset_info->_filters.size());
    for (const auto& filter : asset_info->_filters) {
      logchan_catalog->log("  Filter: %s", filter.c_str());
    }
    // Convert string filters to include filter function
    create_options.include_filter = [asset_info, source_dir](const std::string& path) -> bool {
      // Get relative path from source_dir
      std::string relative_path = path;
      std::string source_dir_str = source_dir.toStdString();
      if (path.find(source_dir_str) == 0) {
        relative_path = path.substr(source_dir_str.length());
        // Remove leading slash if present
        if (!relative_path.empty() && relative_path[0] == '/') {
          relative_path = relative_path.substr(1);
        }
      }
      
      //logchan_catalog->log("  Checking file: %s (relative: %s)", path.c_str(), relative_path.c_str());
      
      // Check if any filter matches this path
      for (const std::string& filter : asset_info->_filters) {
        // Simple glob matching - convert * to regex .*
        std::string regex_pattern = filter;
        
        // Escape special regex characters except *
        size_t pos = 0;
        while ((pos = regex_pattern.find(".", pos)) != std::string::npos) {
          regex_pattern.replace(pos, 1, "\\.");
          pos += 2;
        }
        
        // Convert * to .*
        pos = 0;
        while ((pos = regex_pattern.find("*", pos)) != std::string::npos) {
          regex_pattern.replace(pos, 1, ".*");
          pos += 2;
        }
        
        // Match the pattern
        std::regex pattern(regex_pattern);
        if (std::regex_match(relative_path, pattern)) {
          logchan_catalog->log(" FilterPassed file: %s (relative: %s)", path.c_str(), relative_path.c_str());
          return true;  // Include this file
        }
      }
      
      // If no filter matched, exclude the file
      return false;
    };
  }
  // If no filters specified, include everything (default behavior)

  create_options._deterministic = true;
  auto archive = util::TarArchive::createFromDirectory(source_dir, create_options);
  if (!archive || !archive->isValid()) {
    logchan_catalog->log("packFromLocal: Failed to create tar archive from directory: %s", source_dir.c_str());
    return nullptr;
  }
  auto tar_data = archive->getArchiveData();

  return tar_data;
}

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::asset::catalog {
#endif // ORK_IOS
