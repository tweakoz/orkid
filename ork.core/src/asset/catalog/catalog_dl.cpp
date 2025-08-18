////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <ork/util/logger.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <ork/util/password_provider.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "catalog_impl.h"

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");


////////////////////////////////////////////////////////////////
// Asset Retrieval
////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetCatalog::get(const assetid_t& fq_asset_id, bool decrypt, bool disable_cache) {
  
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
  
  // 4. Create fetch request with all parameters
  auto fetch_request = std::make_shared<FetchRequest>();
  fetch_request->asset_id = fq_asset_id;
  fetch_request->location = *location;
  fetch_request->asset_info = asset_info;
  fetch_request->decrypt = decrypt;
  fetch_request->disable_cache = disable_cache;
  
  // 5. Delegate to CatalogImpl for the actual work
  auto result = impl->getAsset(fetch_request);
  
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
// Async Asset Retrieval
////////////////////////////////////////////////////////////////

assetfuture_ptr_t AssetCatalog::enqueueGet(const assetid_t& fq_asset_id, bool decrypt, bool disable_cache) {
  
  auto impl = _impl.getShared<CatalogImpl>();
  
  // Create the future
  auto future = std::make_shared<AssetFuture>();
  future->_asset_id = fq_asset_id;
  // Don't use shared_from_this - the future doesn't need catalog reference
  
  // 1. Validation - get or create flyweight request
  auto request = mergeAssetReq(fq_asset_id);
  
  // 2. Check if asset exists
  auto asset_info = getAssetInfo(fq_asset_id);
  if (!asset_info) {
    // Asset not found - complete immediately with error
    auto result = std::make_shared<AssetResult>();
    result->_status = AssetStatus::NOT_FOUND;
    result->_error_detail = FormatString("Asset not found: %s", fq_asset_id.c_str());
    request->_state = AssetState::FAILED;
    
    future->_result = result;
    future->_is_complete = true;
    future->_cv.notify_all();
    return future;
  }
  
  // 3. Locate the asset
  auto location = impl->locateAsset(fq_asset_id);
  if (!location) {
    // Failed to locate - complete immediately with error
    auto result = std::make_shared<AssetResult>();
    result->_status = AssetStatus::NOT_FOUND;
    result->_error_detail = "Failed to locate asset";
    
    future->_result = result;
    future->_is_complete = true;
    future->_cv.notify_all();
    return future;
  }
  
  // 4. Handle password authentication upfront (on main thread)
  // This must happen before enqueueing to allow interactive password prompt
  if (location->_location_info) {
    auto& loc_info = location->_location_info;
    if (loc_info->_api_key_read.has_value()) {
      std::string api_key = loc_info->_api_key_read.value();
      
      // Check if this requires password authentication
      if (PasswordProvider::requiresPasswordAuth(api_key)) {
        // Prompt for password NOW on main thread
        std::string host = loc_info->_download_url._host;
        std::string prompt = FormatString("Password for %s: ", host.c_str());
        auto password = PasswordProvider::getPassword(prompt, true); // Allow caching
        
        if (password.has_value()) {
          // Replace the placeholder with actual password
          loc_info->_api_key_read = password.value();
        } else {
          // No password provided - fail immediately
          auto result = std::make_shared<AssetResult>();
          result->_status = AssetStatus::PERMISSION;
          result->_error_detail = "Password authentication required but not provided";
          
          future->_result = result;
          future->_is_complete = true;
          future->_cv.notify_all();
          return future;
        }
      }
    }
  }
  
  // 5. Create fetch request with all parameters
  auto fetch_request = std::make_shared<FetchRequest>();
  fetch_request->asset_id = fq_asset_id;
  fetch_request->location = *location;
  fetch_request->asset_info = asset_info;
  fetch_request->decrypt = decrypt;
  fetch_request->disable_cache = disable_cache;
  
  future->_fetch_request = fetch_request;
  
  // 6. Enqueue the work to be done asynchronously
  // Use the work queue from download manager or create one
  opq::concurrentQueue()->enqueue([impl, fetch_request, future, request]() {
    // Do the actual work
    auto result = impl->getAsset(fetch_request);
    
    // Handle result and update state
    if (result->_status == AssetStatus::OK) {
      request->_state = AssetState::CACHED_MEMORY;
    } else {
      request->_state = AssetState::FAILED;
    }
    
    // Update statistics
    impl->_stats.atomicOp([&](CatalogImpl::Stats& stats) {
      if (result->_status == AssetStatus::OK) {
        stats.cache_misses++;
        stats.bytes_downloaded += result->_bytes_downloaded;
        stats.total_download_time += result->_download_time;
        stats.total_processing_time += result->_processing_time;
      }
    });
    
    // Complete the future
    {
      std::lock_guard<std::mutex> lock(future->_mutex);
      future->_result = result;
      future->_is_complete = true;
    }
    future->_cv.notify_all();
  });
  
  return future;
}

////////////////////////////////////////////////////////////////
// AssetFuture Implementation
////////////////////////////////////////////////////////////////

assetresult_ptr_t AssetFuture::wait() {
  std::unique_lock<std::mutex> lock(_mutex);
  _cv.wait(lock, [this] { return _is_complete.load() || _is_cancelled.load(); });
  
  if (_is_cancelled) {
    if (!_result) {
      _result = std::make_shared<AssetResult>();
      _result->_status = AssetStatus::CANCELLED;
      _result->_error_detail = "Operation was cancelled";
    }
  }
  
  return _result;
}

void AssetFuture::cancel() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _is_cancelled = true;
    
    // If not already complete, create a cancelled result
    if (!_is_complete) {
      _result = std::make_shared<AssetResult>();
      _result->_status = AssetStatus::CANCELLED;
      _result->_error_detail = "Operation was cancelled";
      _is_complete = true;
    }
  }
  _cv.notify_all();
}

assetresult_ptr_t AssetFuture::getResult() const {
  if (_is_complete.load()) {
    return _result;
  }
  return nullptr;
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

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::downloadFile(const URL& url, const locationinfo_ptr_t& location_info) {
  // Atomic file download

  // Handle different URL schemes
  std::string url_str = url.toString();
  if (url_str.find("file://") == 0) {
    // File URL scheme detected
    // Local file URL
    std::string file_path = url_str.substr(7); // Remove "file://" prefix
    // File path extracted

    // Check if file exists
    if (!FileEnv::GetRef().DoesFileExist(file::Path(file_path))) {
      logchan_catalog->log("ERROR: File not found: %s", file_path.c_str());
      return nullptr;
    }

    // Read file
    File file(file::Path(file_path), EFM_READ);
    size_t file_size = 0;
    file.GetLength(file_size);

    auto _data = std::make_shared<DataBlock>();
    _data->reserve(file_size);
    _data->_storage.resize(file_size);

    size_t bytes_read = 0;
    file.Read(const_cast<uint8_t*>(_data->data()), file_size);
    bytes_read = file_size; // Assume success for now

    if (bytes_read != file_size) {
      logchan_catalog->log("ERROR: Failed to read complete file: %s", file_path.c_str());
      return nullptr;
    }

    return _data;

  } else if (url_str.find("http://") == 0 || url_str.find("https://") == 0) {
    // HTTP(S) URL scheme detected
    // HTTP(S) download
    if (_download_manager) {
      // Using download manager
      // Create a temporary file path for the download
      auto temp_dir  = file::Path::temp_dir();
      auto filename  = FormatString("asset_download_%zu.tmp", std::hash<std::string>{}(url_str));
      auto temp_path = temp_dir / filename;

      // Create download object
      auto dl = std::make_shared<Download>(url, temp_path);

      // Configure API key and TLS settings from location
      if (location_info) {
        if (location_info->_api_key_read.has_value() && !location_info->_api_key_read.value().empty()) {
          std::string api_key = location_info->_api_key_read.value();

          // Check if password authentication is required
          if (PasswordProvider::requiresPasswordAuth(api_key)) {
            // Prompt for password
            std::string host   = location_info->_download_url._host;
            std::string prompt = FormatString("Password for %s: ", host.c_str());
            auto password      = PasswordProvider::getPassword(prompt, true); // Allow caching

            if (password.has_value()) {
              dl->setHeader("X-API-Key", password.value());
              logchan_catalog->log("Using password authentication for %s", host.c_str());
            } else {
              logchan_catalog->log("ERROR: Password authentication required but not provided");
              return nullptr;
            }
          } else {
            // Use regular API key
            dl->setHeader("X-API-Key", api_key);
          }
        }
        dl->_ignore_tls_errors = location_info->_disable_cert_check;
      } else {
        printf("[DEBUG] No location_info available, using defaults\n");
      }

      // Set up completion tracking
      std::atomic<bool> download_complete{false};
      std::atomic<bool> download_success{false};
      datablock_ptr_t result_data;

      // Set completion callback
      dl->_on_complete._item = [&](bool success, const file::Path& path) {
        download_success = success;
        if (success) {
          // Read the downloaded file into a DataBlock using stdio
          FILE* fp = fopen(path.c_str(), "rb");
          if (fp) {
            // Get file size
            fseek(fp, 0, SEEK_END);
            size_t file_size = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            result_data = std::make_shared<DataBlock>();
            result_data->reserve(file_size);
            result_data->_storage.resize(file_size);

            size_t bytes_read = fread(const_cast<void*>(static_cast<const void*>(result_data->data())), 1, file_size, fp);
            fclose(fp);

            if (bytes_read != file_size) {
              logchan_catalog->log("ERROR: Failed to read complete downloaded file");
              result_data      = nullptr;
              download_success = false;
            }

            // Delete temporary file
            std::remove(path.c_str());
          } else {
            logchan_catalog->log("ERROR: Failed to open downloaded file: %s", path.c_str());
            download_success = false;
          }
        }
        download_complete = true;
      };

      // Set failure callback
      dl->_on_failure._item = [&](const std::string& error) {
        printf("[DEBUG] Download failure callback called: error=%s\n", error.c_str());
        logchan_catalog->log("ERROR: Download failed: %s", error.c_str());
        download_complete = true;
      };

      _download_manager->enqueue(dl);

      // Wait for download to complete (blocking for sync version)
      int wait_count = 0;
      while (!download_complete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        wait_count++;
      }

      if (!download_success) {
        return nullptr;
      }

      return result_data;
    } else {
      logchan_catalog->log("ERROR: No download manager configured for HTTP: %s", url_str.c_str());
      return nullptr;
    }
  } else {
    // Unknown URL scheme
    logchan_catalog->log("ERROR: Unknown URL scheme: %s", url_str.c_str());
    return nullptr;
  }
}

} //namespace ork::asset::catalog {
