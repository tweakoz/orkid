////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
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
    result->status       = AssetStatus::NOT_FOUND;
    result->error_detail = FormatString("Asset not found: %s", fq_pak_asset_id.c_str());
    return result;
  }

  // 2. Verify it's an asset_pak
  if (asset_info->_type != "asset_pak") {
    result->status       = AssetStatus::UNSUPPORTED;
    result->error_detail = FormatString("Asset is not asset_pak type: %s", asset_info->_type.c_str());
    return result;
  }

  // 3. Get pak contents using regular get() - this handles download/decryption/decompression
  auto pak_result = get(fq_pak_asset_id, true);
  if (!pak_result || !pak_result->isSuccess() || !pak_result->isPak()) {
    result->status       = pak_result ? pak_result->status : AssetStatus::DOWNLOAD_FAILED;
    result->error_detail = pak_result ? pak_result->error_detail : "Failed to retrieve pak";
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
    result->status       = AssetStatus::UNSUPPORTED;
    result->error_detail = "No local location specified for pak asset";
    return result;
  }

  // 5. Extract pak contents to individual files
  extract_path.ensureDirectoryExists();

  for (const auto& [filename, data] : pak_result->pak_contents) {
    if (!data)
      continue;

    auto output_file = extract_path / filename;
    // Ensure parent directory exists
    file::Path parent_path;
    parent_path.fromBFS(output_file.toBFS().parent_path());
    parent_path.ensureDirectoryExists();

    // Write file data
    bool write_success =
        File::writeBinary(output_file, std::vector<uint8_t>((uint8_t*)data->data(), (uint8_t*)data->data() + data->length()));

    if (!write_success) {
      result->status       = AssetStatus::DECOMPRESS_FAILED;
      result->error_detail = FormatString("Failed to write extracted file: %s", output_file.c_str());
      return result;
    }
  }

  // Extraction was successful if we got here

  result->status          = AssetStatus::OK;
  result->processing_time = timer.SecsSinceStart();

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
    result->status       = AssetStatus::NOT_FOUND;
    result->error_detail = FormatString("Asset not found: %s", fq_pak_asset_id.c_str());
    return result;
  }

  logchan_catalog->log("packFromLocal: Asset ID=%s, local_loc=%s, filename=%s", 
                       fq_pak_asset_id.c_str(), asset_info->_local_loc.c_str(), asset_info->_filename.c_str());

  // 2. Verify it's an asset_pak
  if (asset_info->_type != "asset_pak") {
    result->status       = AssetStatus::UNSUPPORTED;
    result->error_detail = FormatString("Asset is not asset_pak type: %s", asset_info->_type.c_str());
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
    result->status       = AssetStatus::UNSUPPORTED;
    result->error_detail = "No local location specified for pak asset";
    return result;
  }

  // 4. Get directory name by removing .tar extension from filename
  std::string dir_name = asset_info->_filename;
  if (dir_name.size() > 4 && dir_name.substr(dir_name.size() - 4) == ".tar") {
    dir_name = dir_name.substr(0, dir_name.size() - 4);
  }
  
  // 5. Look for the source directory
  file::Path source_dir = pak_local_path / dir_name;
  
  logchan_catalog->log("packFromLocal: Looking for directory: %s (pak_local_path=%s, dir_name=%s)", 
                       source_dir.c_str(), pak_local_path.c_str(), dir_name.c_str());
  
  if (!source_dir.doesPathExist()) {
    result->status       = AssetStatus::NOT_FOUND;
    result->error_detail = FormatString("Source directory not found: %s (pak_local_path=%s, dir_name=%s)", 
                                       source_dir.c_str(), pak_local_path.c_str(), dir_name.c_str());
    return result;
  }

  // 6. Create TAR from directory contents
  util::TarCreateOptions create_options;
  create_options.base_path = source_dir.c_str();

  auto archive = util::TarArchive::createFromDirectory(source_dir, create_options);
  if (!archive || !archive->isValid()) {
    result->status       = AssetStatus::DOWNLOAD_FAILED;
    result->error_detail = FormatString("Failed to create tar archive from directory: %s", source_dir.c_str());
    return result;
  }

  // 6. Get pak data
  result->data = archive->getArchiveData();
  if (!result->data) {
    result->status       = AssetStatus::DOWNLOAD_FAILED;
    result->error_detail = "Failed to get archive data";
    return result;
  }

  result->status          = AssetStatus::OK;
  result->processing_time = timer.SecsSinceStart();

  return result;
}

////////////////////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetCatalog::packFromLocal(assetentry_ptr_t asset_info) {
  auto result = std::make_shared<AssetResult>();
  Timer timer;
  timer.Start();

  // 1. Verify we have asset info
  if (!asset_info) {
    result->status       = AssetStatus::NOT_FOUND;
    result->error_detail = "No asset info provided";
    return result;
  }

  logchan_catalog->log("packFromLocal(direct): local_loc=%s, filename=%s", 
                       asset_info->_local_loc.c_str(), asset_info->_filename.c_str());

  // 2. Verify it's an asset_pak
  if (asset_info->_type != "asset_pak") {
    result->status       = AssetStatus::UNSUPPORTED;
    result->error_detail = FormatString("Asset is not asset_pak type: %s", asset_info->_type.c_str());
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
    result->status       = AssetStatus::UNSUPPORTED;
    result->error_detail = "No local location specified for pak asset";
    return result;
  }

  // 4. Get directory name by removing .tar extension from filename
  std::string dir_name = asset_info->_filename;
  if (dir_name.size() > 4 && dir_name.substr(dir_name.size() - 4) == ".tar") {
    dir_name = dir_name.substr(0, dir_name.size() - 4);
  }
  
  // 5. Look for the source directory
  file::Path source_dir = pak_local_path / dir_name;
  
  logchan_catalog->log("packFromLocal: Looking for directory: %s (pak_local_path=%s, dir_name=%s)", 
                       source_dir.c_str(), pak_local_path.c_str(), dir_name.c_str());
  
  if (!source_dir.doesPathExist()) {
    result->status       = AssetStatus::NOT_FOUND;
    result->error_detail = FormatString("Source directory not found: %s (pak_local_path=%s, dir_name=%s)", 
                                       source_dir.c_str(), pak_local_path.c_str(), dir_name.c_str());
    return result;
  }

  // 6. Create TAR from directory contents
  util::TarCreateOptions create_options;
  create_options.base_path = source_dir.c_str();

  auto archive = util::TarArchive::createFromDirectory(source_dir, create_options);
  if (!archive || !archive->isValid()) {
    result->status       = AssetStatus::DOWNLOAD_FAILED;
    result->error_detail = FormatString("Failed to create tar archive from directory: %s", source_dir.c_str());
    return result;
  }

  // 7. Get pak data
  result->data = archive->getArchiveData();
  if (!result->data) {
    result->status       = AssetStatus::DOWNLOAD_FAILED;
    result->error_detail = "Failed to get archive data";
    return result;
  }

  result->status          = AssetStatus::OK;
  result->processing_time = timer.SecsSinceStart();

  return result;
}

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::asset::catalog {
