////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/request.h>
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

assetfqid_ptr_t AssetCatalog::findAsset(const assetid_t& fq_asset_id) const {
  auto impl = _impl.getShared<CatalogImpl>();
  auto location = impl->locateAsset(fq_asset_id);
  auto asset_index_entry = findAssetIndexEntry(fq_asset_id);

  if ((asset_index_entry==nullptr) or (location==nullptr)) {
    logchan_catalog->log("[DEBUG] Asset<%s> or location not found in catalog", fq_asset_id.c_str());
    return nullptr;
  }
  auto asset_info = asset_index_entry->_entry;
  auto fqID = std::make_shared<AssetFqIdentifier>();
  fqID->_original_fqid = fq_asset_id;
  fqID->_namespace_id = asset_info->_namespace;
  fqID->_asset_id = asset_info->_id;
  fqID->_location = location;
  fqID->_asset_info = asset_info;
  fqID->_namespace = findNamespace(fqID->_namespace_id);
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
    //logchan_catalog->log("packFromLocal: Asset ID has empty _local_loc: %s", fq_pak_asset_id.c_str());
    //return false;
  }
  fqID->_pak_local_path = pak_local_path;
  // 4. Determine source directory using tar_root field
  file::Path source_dir;
  if (asset_info->_tar_root.empty()) {
    // No tar_root specified - use pak_local_path directly
    source_dir = pak_local_path;
  } else {
    // Use tar_root to find the source directory
    source_dir = pak_local_path / asset_info->_tar_root;
  }
  fqID->_source_dir = source_dir;
  return fqID;
}

////////////////////////////////////////////////////////////////
// Asset Retrieval
////////////////////////////////////////////////////////////////

fetchrequest_ptr_t AssetCatalog::fetch( const assetid_t& fq_asset_id, //
                                        bool enable_caching) {         //
  ////////////////////////////////////////
  // async fetch
  ////////////////////////////////////////
  auto request = fetchAsync(fq_asset_id,enable_caching);
  if (!request) {
    return nullptr;
  }
  ////////////////////////////////////////
  // wait (synchronous)
  ////////////////////////////////////////
  bool OK = request->wait();
  return request;
}

////////////////////////////////////////////////////////////////
// Async Asset Retrieval
////////////////////////////////////////////////////////////////

fetchrequest_ptr_t AssetCatalog::fetchAsync(const assetid_t& fq_asset_id, //
                                            bool enable_caching) { //
  ////////////////////////////////////////
  // find asset from catalog
  ////////////////////////////////////////
  auto impl = _impl.getShared<CatalogImpl>();
  auto fqid = findAsset(fq_asset_id);
  if(nullptr==fqid) {
    return nullptr;
  }
  ////////////////////////////////////////
  // 1. flyweighted request
  ////////////////////////////////////////
  auto request = _mergeRequest(fqid);
  if (!request) {
    return nullptr;
  }
  ////////////////////////////////////////
  // check request state 
  //  (only proceed if NEW)
  ////////////////////////////////////////

  switch( request->_state.load() ) {
    case AssetState::NEW:
      break;
    case AssetState::ENQUEUE_PENDING:
    case AssetState::ENQUEUED:
    case AssetState::DOWNLOADING:
    case AssetState::PROCESSING:
    case AssetState::SUCCEEDED:
      // Already completed or invalid
      return request;
    default:
      OrkAssert(false);
      return request;
  }

  request->_enable_caching = enable_caching;

  ////////////////////////////////////////
  // New Request. Proceed to enqueue.
  ////////////////////////////////////////
  auto location = fqid->_location;
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
          return nullptr;
        }
      }
    }
  }
  
  ////////////////////////////////////////
  // 5. Create fetch request with all parameters
  ////////////////////////////////////////
  
  // 6. Enqueue the work to be done asynchronously
  // Use the work queue from download manager or create one
  opq::concurrentQueue()->enqueue([impl, request]() {
    // Do the actual work
    // Handle result and update state
    if (impl->getAsset(request)) { // synchronous call
      request->_state = AssetState::SUCCEEDED;
    } else {
      request->_state = AssetState::FAILED;
    }
    FetchRequest::invokeCompletionCallbacks(request);
    // Update statistics
    impl->_stats.atomicOp([&](CatalogImpl::Stats& stats) {
      if (request->_status == AssetStatus::OK) {
        stats.cache_misses++;
        stats.bytes_downloaded += request->_bytes_downloaded;
        stats.total_download_time += request->_download_time;
        stats.total_processing_time += request->_processing_time;
      }
    });    
  });

  return request;
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

datablock_ptr_t CatalogImpl::_downloadFile(const URL& url, const locationinfo_ptr_t& location_info) {
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
        printf("[DEBUG]   url=%s\n", url.toString().c_str());

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
