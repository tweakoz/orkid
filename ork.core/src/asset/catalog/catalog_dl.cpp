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
// Asset Retrieval
////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetCatalog::get(const assetid_t& fq_asset_id, bool decrypt) {
  
  auto impl = _impl.getShared<CatalogImpl>();
  
  // 1. Validation - get or create flyweight request
  auto request = mergeAssetReq(fq_asset_id);
  
  // 2. Check if asset exists
  auto asset_info = getAssetInfo(fq_asset_id);
  if (!asset_info) {
    printf("[DEBUG] Asset not found in catalog\n");
    auto result = std::make_shared<AssetResult>();
    result->_status = AssetStatus::NOT_FOUND;
    result->_error_detail = FormatString("Asset not found: %s", fq_asset_id.c_str());
    request->_state = AssetState::FAILED;
    return result;
  }
  
  // 3. Locate the asset
  auto location = impl->locateAsset(fq_asset_id);
  if (!location) {
    printf("[DEBUG] Failed to locate asset\n");
    auto result = std::make_shared<AssetResult>();
    result->_status = AssetStatus::NOT_FOUND;
    result->_error_detail = "Failed to locate asset";
    return result;
  }
  
  // 4. Delegate to CatalogImpl for the actual work
  auto result = impl->getAsset(fq_asset_id, *location, asset_info, decrypt);
  
  // 5. Handle result and update state
  if (result->_status == AssetStatus::OK) {
    request->_state = AssetState::CACHED_MEMORY;
  } else {
    request->_state = AssetState::FAILED;
  }
  
  // 6. Update statistics
  impl->_stats.atomicOp([&](CatalogImpl::Stats& stats) {
    if (result->_status == AssetStatus::OK) {
      stats.cache_misses++;
      stats.bytes_downloaded += result->_bytes_downloaded;
      stats.total_download_time += result->_download_time;
      stats.total_processing_time += result->_processing_time;
    }
  });
  
  return result;
}

////////////////////////////////////////////////////////////////
// Download Management
////////////////////////////////////////////////////////////////


void AssetCatalog::cancelDownload(chunkdownloadcoordinator_ptr_t coordinator) {
  if (!coordinator)
    return;

  // Cancel the download operation
  coordinator->cancel();

  // TODO: Remove from _coordinators_by_assetid tracking
  auto impl = _impl.getShared<CatalogImpl>();
  impl->_coordinators_by_assetid.atomicOp([&](CatalogImpl::chunk_coordinator_map_t& map) {
    auto it = map.find(coordinator->asset_id);
    if (it != map.end() && it->second == coordinator) {
      map.erase(it);
    }
  });
}


////////////////////////////////////////////////////////////////
// DownloadProgress
////////////////////////////////////////////////////////////////


std::string DownloadProgress::getRateString() const {
  if (_rate < 1024) {
    return FormatString("%.0f B/s", _rate);
  } else if (_rate < 1024 * 1024) {
    return FormatString("%.1f KB/s", _rate / 1024.0);
  } else {
    return FormatString("%.1f MB/s", _rate / (1024.0 * 1024.0));
  }
}


} //namespace ork::asset::catalog {
