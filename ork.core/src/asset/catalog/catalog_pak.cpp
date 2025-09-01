////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/manifest.h>
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

////////////////////////////////////////////////////////////////
// Asset Pak Operations
////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetCatalog::unpackToLocal(const assetid_t& fq_pak_asset_id) {
  auto result = std::make_shared<AssetResult>();
  Timer timer;
  timer.Start();

  // 1. Get pak asset info
  assetentry_ptr_t asset_info = getAssetInfo(fq_pak_asset_id);
  if (!asset_info) {
    result->_status       = AssetStatus::NOT_FOUND;
    result->_error_detail = FormatString("Asset not found: %s", fq_pak_asset_id.c_str());
    return result;
  }

  // 2. Verify it's an asset_pak
  if (asset_info->_type != "asset_pak") {
    result->_status       = AssetStatus::UNSUPPORTED;
    result->_error_detail = FormatString("Asset is not asset_pak type: %s", asset_info->_type.c_str());
    return result;
  }

  // 3. Get pak contents using regular get() - this handles download/decryption/decompression
  auto pak_result = fetch(fq_pak_asset_id, true);
  if (!pak_result || !pak_result->isSuccess() || !pak_result->isPak()) {
    result->_status       = pak_result ? pak_result->_status : AssetStatus::DOWNLOAD_FAILED;
    result->_error_detail = pak_result ? pak_result->_error_detail : "Failed to retrieve pak";
    return result;
  }

  // 4. Determine extraction directory from _local_loc
  file::Path extract_path;
  if (!asset_info->_local_loc.empty()) {
    // Parse file:// URL to get directory path
    std::string local_url = asset_info->_local_loc;
    if (local_url.find("file://") == 0) {
      extract_path = file::Path(local_url.substr(7)); // Remove "file://"
    } else {
      extract_path = file::Path(local_url);
    }
  } else {
    result->_status       = AssetStatus::UNSUPPORTED;
    result->_error_detail = "No local location specified for pak asset";
    return result;
  }

  // 5. Extract pak contents to individual files
  extract_path.ensureDirectoryExists();

  for (const auto& [filename, _data] : pak_result->_pak_contents) {
    if (!_data)
      continue;

    auto output_file = extract_path / filename;
    // Ensure parent directory exists
    file::Path parent_path;
    parent_path.fromBFS(output_file.toBFS().parent_path());
    parent_path.ensureDirectoryExists();

    // Write file data
    bool write_success =
        File::writeBinary(output_file, std::vector<uint8_t>((uint8_t*)_data->data(), (uint8_t*)_data->data() + _data->length()));

    if (!write_success) {
      result->_status       = AssetStatus::DECOMPRESS_FAILED;
      result->_error_detail = FormatString("Failed to write extracted file: %s", output_file.c_str());
      return result;
    }
  }

  // Extraction was successful if we got here

  result->_status          = AssetStatus::OK;
  result->_processing_time = timer.SecsSinceStart();

  return result;
}

////////////////////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetCatalog::packFromLocal(const assetid_t& fq_pak_asset_id) {
  auto result = std::make_shared<AssetResult>();
  Timer timer;
  timer.Start();

  // 1. Get pak asset info
  assetentry_ptr_t asset_info = getAssetInfo(fq_pak_asset_id);
  if (!asset_info) {
    result->_status       = AssetStatus::NOT_FOUND;
    result->_error_detail = FormatString("Asset not found: %s", fq_pak_asset_id.c_str());
    return result;
  }

  logchan_catalog->log("packFromLocal: Asset ID=%s, local_loc=%s", 
                       fq_pak_asset_id.c_str(), asset_info->_local_loc.c_str());

  // 2. Verify it's an asset_pak
  if (asset_info->_type != "asset_pak") {
    result->_status       = AssetStatus::UNSUPPORTED;
    result->_error_detail = FormatString("Asset is not asset_pak type: %s", asset_info->_type.c_str());
    return result;
  }

  // 3. Determine source directory from _local_loc with template resolution
  file::Path pak_local_path;
  if (!asset_info->_local_loc.empty()) {
    // Resolve template paths like <cache>, <stage> 
    std::string resolved_local = asset_info->_local_loc;
    
    // Use the same resolution logic as AssetEntry::getResolvedLocalPath()
    if (resolved_local.find("<stage>") == 0) {
      resolved_local.replace(0, 7, file::Path::stage_dir().c_str());
    } else if (resolved_local.find("<assetcache>") == 0) {
      std::string cache_path = (file::Path::stage_dir() / "assetcache").c_str();
      resolved_local.replace(0, 12, cache_path);
    } else if (resolved_local.find("<cache>") == 0) {
      std::string cache_path = (file::Path::stage_dir() / "assetcache").c_str();
      resolved_local.replace(0, 7, cache_path);
    }
    
    // Handle file:// URLs
    if (resolved_local.find("file://") == 0) {
      pak_local_path = file::Path(resolved_local.substr(7)); // Remove "file://"
    } else {
      pak_local_path = file::Path(resolved_local);
    }
  } else {
    result->_status       = AssetStatus::UNSUPPORTED;
    result->_error_detail = "No local location specified for pak asset";
    return result;
  }

  // 4. Determine source directory using tar_root field
  file::Path source_dir;
  if (asset_info->_tar_root.empty()) {
    // No tar_root specified - use pak_local_path directly
    source_dir = pak_local_path;
  } else {
    // Use tar_root to find the source directory
    source_dir = pak_local_path / asset_info->_tar_root;
  }
  
  logchan_catalog->log("packFromLocal: Looking for directory: %s (pak_local_path=%s, tar_root=%s)", 
                       source_dir.c_str(), pak_local_path.c_str(), asset_info->_tar_root.c_str());
  
  if (!source_dir.doesPathExist()) {
    result->_status       = AssetStatus::NOT_FOUND;
    result->_error_detail = FormatString("Source directory not found: %s (pak_local_path=%s, tar_root=%s)", 
                                       source_dir.c_str(), pak_local_path.c_str(), asset_info->_tar_root.c_str());
    return result;
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

  auto archive = util::TarArchive::createFromDirectory(source_dir, create_options);
  if (!archive || !archive->isValid()) {
    result->_status       = AssetStatus::DOWNLOAD_FAILED;
    result->_error_detail = FormatString("Failed to create tar archive from directory: %s", source_dir.c_str());
    return result;
  }

  // 6. Get pak data
  result->_data = archive->getArchiveData();
  if (!result->_data) {
    result->_status       = AssetStatus::DOWNLOAD_FAILED;
    result->_error_detail = "Failed to get archive _data";
    return result;
  }

  result->_status          = AssetStatus::OK;
  result->_processing_time = timer.SecsSinceStart();

  return result;
}

////////////////////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetCatalog::packFromLocal(assetentry_ptr_t asset_info) {
  auto result = std::make_shared<AssetResult>();
  Timer timer;
  timer.Start();

  // 1. Verify we have asset info
  if (!asset_info) {
    result->_status       = AssetStatus::NOT_FOUND;
    result->_error_detail = "No asset info provided";
    return result;
  }

  logchan_catalog->log("packFromLocal(direct): local_loc=%s", 
                       asset_info->_local_loc.c_str());

  // 2. Verify it's an asset_pak
  if (asset_info->_type != "asset_pak") {
    result->_status       = AssetStatus::UNSUPPORTED;
    result->_error_detail = FormatString("Asset is not asset_pak type: %s", asset_info->_type.c_str());
    return result;
  }

  // 3. Determine source directory from _local_loc with template resolution
  file::Path pak_local_path;
  if (!asset_info->_local_loc.empty()) {
    // Resolve template paths like <cache>, <stage> 
    std::string resolved_local = asset_info->_local_loc;
    
    // Use the same resolution logic as AssetEntry::getResolvedLocalPath()
    if (resolved_local.find("<stage>") == 0) {
      resolved_local.replace(0, 7, file::Path::stage_dir().c_str());
    } else if (resolved_local.find("<assetcache>") == 0) {
      std::string cache_path = (file::Path::stage_dir() / "assetcache").c_str();
      resolved_local.replace(0, 12, cache_path);
    } else if (resolved_local.find("<cache>") == 0) {
      std::string cache_path = (file::Path::stage_dir() / "assetcache").c_str();
      resolved_local.replace(0, 7, cache_path);
    }
    
    // Handle file:// URLs
    if (resolved_local.find("file://") == 0) {
      pak_local_path = file::Path(resolved_local.substr(7)); // Remove "file://"
    } else {
      pak_local_path = file::Path(resolved_local);
    }
  } else {
    result->_status       = AssetStatus::UNSUPPORTED;
    result->_error_detail = "No local location specified for pak asset";
    return result;
  }

  // 4. Determine source directory using tar_root field
  file::Path source_dir;
  if (asset_info->_tar_root.empty()) {
    // No tar_root specified - use pak_local_path directly
    source_dir = pak_local_path;
  } else {
    // Use tar_root to find the source directory
    source_dir = pak_local_path / asset_info->_tar_root;
  }
  
  logchan_catalog->log("packFromLocal: Looking for directory: %s (pak_local_path=%s, tar_root=%s)", 
                       source_dir.c_str(), pak_local_path.c_str(), asset_info->_tar_root.c_str());
  
  if (!source_dir.doesPathExist()) {
    result->_status       = AssetStatus::NOT_FOUND;
    result->_error_detail = FormatString("Source directory not found: %s (pak_local_path=%s, tar_root=%s)", 
                                       source_dir.c_str(), pak_local_path.c_str(), asset_info->_tar_root.c_str());
    return result;
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

  auto archive = util::TarArchive::createFromDirectory(source_dir, create_options);
  if (!archive || !archive->isValid()) {
    result->_status       = AssetStatus::DOWNLOAD_FAILED;
    result->_error_detail = FormatString("Failed to create tar archive from directory: %s", source_dir.c_str());
    return result;
  }

  // 7. Get pak data
  result->_data = archive->getArchiveData();
  if (!result->_data) {
    result->_status       = AssetStatus::DOWNLOAD_FAILED;
    result->_error_detail = "Failed to get archive _data";
    return result;
  }

  result->_status          = AssetStatus::OK;
  result->_processing_time = timer.SecsSinceStart();

  return result;
}

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::asset::catalog {
