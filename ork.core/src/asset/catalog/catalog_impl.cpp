
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
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

////////////////////////////////////////////////////////////////
// Internal Methods
////////////////////////////////////////////////////////////////
namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

void CatalogImpl::rebuildAssetIndex() {
  // TODO: Implement
}

assetlocation_ptr_t CatalogImpl::locateAsset(const assetid_t& fq_asset_id) const {
  assetlocation_ptr_t result;
  
  _state.atomicOp([&](const CatalogState& state) {
    auto it = state._entries_by_assetid.find(fq_asset_id);
    if (it != state._entries_by_assetid.end()) {
      result = std::make_shared<AssetLocation>();
      result->namespace_id = it->second.namespace_id;
      result->relative_path = it->second.asset_path;
      result->source_manifest = it->second.manifest;
      // All CDN content is encrypted (system invariant)
      result->is_encrypted = true;
      result->is_compressed = it->second.entry->_is_compressed;
      result->compression_type = it->second.entry->_compression_type;
      result->chunk_manifest = it->second.entry->_chunk_manifest;
      
      // Build location info directly from entry data
      std::string remote_loc = it->second.entry->_remote_loc;
      std::string storage_hash = it->second.entry->_storage_hash;
      
      if (!remote_loc.empty() && !storage_hash.empty()) {
        // Set base_url to the remote location (which may be a template like <unidevcdn>)
        std::string base_url = remote_loc;
        
        // Resolve location template if present (e.g., <unidevcdn> -> https://localhost:8443)
        if (base_url.find("<") == 0 && base_url.find(">") != std::string::npos) {
          // Extract the location key from template
          size_t end_pos = base_url.find(">");
          std::string location_key = base_url.substr(1, end_pos - 1);
          
          // Get config for this namespace and resolve the location
          if (_config_space) {
            auto configs = _config_space->_configs;
            // Location key resolution logged at higher level if needed
            for (const auto& [config_id, config] : configs) {
              auto location_info = config->resolveRemoteLocation(location_key);
              if (location_info) {
                std::string resolved_url = location_info->url.toString();
                
                // Check if URL was actually resolved (not still a template)
                if ((resolved_url.find("<") == 0 && resolved_url.find(">") != std::string::npos) ||
                    resolved_url == location_key) {
                  printf("[DEBUG] WARNING: URL is still a template: %s - continuing to next config\n", resolved_url.c_str());
                  continue;  // Skip this config, try next one
                }
                
                // Location found logged at higher level if needed
                base_url = resolved_url;
                result->location_info = location_info;  // Store the location_info
                break;
              } else {
                printf("[DEBUG] Config %s has no location for %s\n", config_id.c_str(), location_key.c_str());
              }
            }
          }
        }
        
        result->base_url = base_url;
        result->relative_path = storage_hash + ".enc";
      } else if (!it->second.entry->_local_loc.empty()) {
        // Fallback to local location if remote location is empty
        result->base_url = it->second.entry->_local_loc;
      }
    }
  });
  
  return result;
}

datablock_ptr_t CatalogImpl::downloadFile(const std::string& url, const locationinfo_ptr_t& location_info) {
  // Atomic file download
  
  // Handle different URL schemes
  if (url.find("file://") == 0) {
    // File URL scheme detected
    // Local file URL
    std::string file_path = url.substr(7); // Remove "file://" prefix
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
    
    auto data = std::make_shared<DataBlock>();
    data->reserve(file_size);
    data->_storage.resize(file_size);
    
    size_t bytes_read = 0;
    file.Read(const_cast<uint8_t*>(data->data()), file_size);
    bytes_read = file_size; // Assume success for now
    
    if (bytes_read != file_size) {
      logchan_catalog->log("ERROR: Failed to read complete file: %s", file_path.c_str());
      return nullptr;
    }
    
    return data;
    
  } else if (url.find("http://") == 0 || url.find("https://") == 0) {
    // HTTP(S) URL scheme detected
    // HTTP(S) download
    if (_download_manager) {
      // Using download manager
      // Create a temporary file path for the download
      auto temp_dir = file::Path::temp_dir();
      auto filename = FormatString("asset_download_%zu.tmp", std::hash<std::string>{}(url));
      auto temp_path = temp_dir / filename;
      
      // Create download object
      auto dl = std::make_shared<Download>(URL(url), temp_path);
      
      // Configure API key and TLS settings from location
      if (location_info) {
        if (location_info->api_key.has_value()) {
          dl->setHeader("X-API-Key", location_info->api_key.value());
        }
        dl->_ignore_tls_errors = location_info->disable_cert_check;
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
              result_data = nullptr;
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
      logchan_catalog->log("ERROR: No download manager configured for HTTP: %s", url.c_str());
      return nullptr;
    }
  } else {
    // Unknown URL scheme
    logchan_catalog->log("ERROR: Unknown URL scheme: %s", url.c_str());
    return nullptr;
  }
}

// New refactored methods

assetresult_ptr_t CatalogImpl::getAsset(
    const assetid_t& fq_asset_id,
    const AssetLocation& location,
    const assetentry_ptr_t& asset_info,
    bool decrypt) {
  
  auto result = std::make_shared<AssetResult>();
  result->location = location;
  Timer overall_timer;
  overall_timer.Start();
  
  // 1. Download phase
  Timer download_timer;
  download_timer.Start();
  
  auto raw_data = downloadAssetData(location);
  if (!raw_data) {
    printf("[DEBUG CatalogImpl] Download phase FAILED\n");
    result->status = AssetStatus::DOWNLOAD_FAILED;
    result->error_detail = "Failed to download asset data";
    return result;
  }
  
  result->download_time = download_timer.SecsSinceStart();
  result->bytes_downloaded = raw_data->length();
  
  // 2. Process phase
  Timer process_timer;
  process_timer.Start();
  
  auto processed_data = processAssetData(raw_data, location, decrypt);
  if (!processed_data) {
    printf("[DEBUG CatalogImpl] Process phase FAILED\n");
    // processAssetData doesn't set status, so set it here
    if (decrypt && location.is_encrypted) {
      result->status = AssetStatus::DECRYPT_FAILED;
      result->error_detail = "Failed to decrypt asset";
    } else if (location.is_compressed) {
      result->status = AssetStatus::DECOMPRESS_FAILED;
      result->error_detail = "Failed to decompress asset";
    }
    return result;
  }
  
  result->processing_time = process_timer.SecsSinceStart();
  
  // 3. Handle by type
  if (asset_info->_type == "asset_pak") {
    handleAssetPak(processed_data, *result);
    // Also write to local location if available
    if (!asset_info->_local_loc.empty()) {
      writeAssetPakToLocal(asset_info, *result);
    }
  } else {
    handleRegularAsset(processed_data, *result);
  }
  
  return result;
}

datablock_ptr_t CatalogImpl::downloadAssetData(const AssetLocation& location) {
  if (location.chunk_manifest) {
    return downloadChunkedData(location);
  } else {
    return downloadSingleData(location);
  }
}

datablock_ptr_t CatalogImpl::downloadSingleData(const AssetLocation& location) {
  std::string url = location.base_url;
  if (!url.empty() && url.back() != '/') {
    url += "/";
  }
  url += location.relative_path;
  
  return downloadFile(url, location.location_info);
}

datablock_ptr_t CatalogImpl::downloadChunkedData(const AssetLocation& location) {
  std::vector<datablock_ptr_t> chunks;
  std::string base_url = location.base_url;
  if (!base_url.empty() && base_url.back() != '/') {
    base_url += "/";
  }
  
  for (size_t i = 0; i < location.chunk_manifest->chunks.size(); ++i) {
    std::string chunk_url = base_url + FormatString("%s.chunk.%04zu", 
                                                    location.relative_path.c_str(), i);
    
    auto chunk = downloadFile(chunk_url, location.location_info);
    if (!chunk) {
      printf("[ERROR] Failed to download chunk %zu\n", i);
      return nullptr;
    }
    chunks.push_back(chunk);
  }
  
  ChunkAssembler::Config assembler_config;
  ChunkAssembler assembler(location.chunk_manifest, nullptr, assembler_config);
  auto result = assembler.assembleFromChunks(chunks);
  
  if (!result->success) {
    printf("[ERROR] Chunk assembly failed: %s\n", result->error_message.c_str());
    return nullptr;
  }
  
  return result->assembled_data;
}

datablock_ptr_t CatalogImpl::processAssetData(
    datablock_ptr_t data,
    const AssetLocation& location,
    bool decrypt) {
  
  auto result = data;
  
  // Decrypt if needed
  if (decrypt && location.is_encrypted) {
    result = decryptData(result, location.namespace_id);
    if (!result) {
      printf("[ERROR] Decryption failed\n");
      return nullptr;
    }
  }
  
  // Decompress if needed
  if (location.is_compressed) {
    result = decompressData(result, location.compression_type);
    if (!result) {
      printf("[ERROR] Decompression failed\n");
      return nullptr;
    }
  }
  
  return result;
}

datablock_ptr_t CatalogImpl::decryptData(
    datablock_ptr_t data,
    const namespaceid_t& namespace_id) {
  
  auto codec = _catalog->codecForNamespace(namespace_id);
  if (!codec) {
    printf("[ERROR] No codec available for namespace: %s\n", namespace_id.c_str());
    logchan_catalog->log("ERROR: No codec available for namespace: %s", namespace_id.c_str());
    return nullptr;
  }
  
  auto result = codec->decrypt(data.get());
  if (!result) {
    printf("[ERROR] Decryption failed for namespace: %s\n", namespace_id.c_str());
  }
  return result;
}

datablock_ptr_t CatalogImpl::decompressData(
    datablock_ptr_t data,
    CompressionType compression_type) {
  
  return data->decompressed();
}

void CatalogImpl::handleAssetPak(datablock_ptr_t data, AssetResult& result) {
  
  // Extract tar contents
  auto archive = util::TarArchive::loadFromMemory(data);
  if (!archive || !archive->isValid()) {
    printf("[ERROR] Failed to parse tar archive\n");
    result.status = AssetStatus::DECOMPRESS_FAILED;
    result.error_detail = "Failed to parse tar archive";
    return;
  }
  
  // Extract all entries to memory
  util::TarExtractOptions extract_options;
  auto extracted_entries = archive->extractToMemory(extract_options);
  if (extracted_entries.empty()) {
    printf("[ERROR] No entries found in tar archive\n");
    result.status = AssetStatus::DECOMPRESS_FAILED;
    result.error_detail = "No entries found in tar archive";
    return;
  }
  
  // Convert tar entries to AssetResult format
  for (const auto& [filename, entry] : extracted_entries) {
    if (entry && entry->data) {
      result.pak_contents[filename] = entry->data;
      printf("[DEBUG] Extracted: %s (%zu bytes)\n", filename.c_str(), entry->data->length());
    }
  }
  
  result.status = AssetStatus::OK;
  printf("[DEBUG] Asset pak extraction complete: %zu files\n", result.pak_contents.size());
}

void CatalogImpl::handleRegularAsset(datablock_ptr_t data, AssetResult& result) {
  result.data = data;
  result.status = AssetStatus::OK;
  result.bytes_downloaded = data->length();
}

void CatalogImpl::writeAssetPakToLocal(const assetentry_ptr_t& asset_info, AssetResult& result) {
  printf("[DEBUG] Writing asset pak to local location: %s\n", asset_info->_local_loc.c_str());
  
  // Resolve local path
  file::Path local_path = asset_info->getResolvedLocalPath();
  if (local_path.empty()) {
    printf("[ERROR] Failed to resolve local path\n");
    return;
  }
  
  // For asset_pak, we need to extract the TAR contents
  // The directory name is the filename without .tar extension
  std::string dir_name = asset_info->_filename;
  if (dir_name.size() > 4 && dir_name.substr(dir_name.size() - 4) == ".tar") {
    dir_name = dir_name.substr(0, dir_name.size() - 4);
  }
  
  file::Path extract_dir = local_path;
  printf("[DEBUG] Extracting to directory: %s\n", extract_dir.c_str());
  
  // Ensure directory exists
  extract_dir.ensureDirectoryExists();
  
  // Write each file from pak_contents
  for (const auto& [filename, data] : result.pak_contents) {
    if (!data) continue;
    
    file::Path file_path = extract_dir / filename;
    printf("[DEBUG] Writing file: %s (%zu bytes)\n", file_path.c_str(), data->length());
    
    // Ensure parent directory exists
    namespace fs = boost::filesystem;
    fs::path boost_file_path = file_path.toBFS();
    fs::create_directories(boost_file_path.parent_path());
    
    // Write file using stdio
    FILE* fp = fopen(file_path.c_str(), "wb");
    if (fp) {
      size_t written = fwrite(data->data(), 1, data->length(), fp);
      fclose(fp);
      if (written != data->length()) {
        printf("[ERROR] Failed to write complete file: %s\n", file_path.c_str());
      }
    } else {
      printf("[ERROR] Failed to open file for writing: %s\n", file_path.c_str());
    }
  }
  
  printf("[DEBUG] Asset pak extraction to local complete\n");
}

chunkdownloadcoordinator_ptr_t CatalogImpl::downloadChunkedAsset(
    const assetid_t& fq_asset_id,
    const AssetLocation& location,
    const pysafe_completion_callback_t& on_complete,
    const pysafe_error_callback_t& on_error) {
  // TODO: Implement async version
  return nullptr;
}
chunkdownloadcoordinator_ptr_t CatalogImpl::downloadNonChunkedAsset(
    const assetid_t& fq_asset_id,
    const AssetLocation& location,
    const pysafe_completion_callback_t& on_complete,
    const pysafe_error_callback_t& on_error) {
  // TODO: Implement
  return nullptr;
}

void CatalogImpl::processDownloadTask(const DownloadTask& task) {
  if (_download_manager && _download_manager->_work_queue) {
    // Queue the task on the download manager's work queue
    _download_manager->_work_queue->enqueue(task.task);
  } else {
    logchan_catalog->log("ERROR: No download manager or work queue available for task");
  }
}

void CatalogImpl::updateDownloadProgress(
    const assetid_t& asset_id,
    size_t current,
    size_t total) {
  // TODO: Implement
}

std::regex CatalogImpl::wildcardToRegex(const std::string& pattern) {
  std::string regex_str;
  for (char c : pattern) {
    switch (c) {
      case '*': regex_str += ".*"; break;
      case '?': regex_str += "."; break;
      case '.': regex_str += "\\."; break;
      case '\\': regex_str += "\\\\"; break;
      default: regex_str += c; break;
    }
  }
  return std::regex(regex_str);
}

} //namespace ork::asset::catalog {
