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
#include <ork/util/download_group.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>
#include "catalog_impl.h"

////////////////////////////////////////////////////////////////
// Internal Methods
////////////////////////////////////////////////////////////////

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////

assetresult_ptr_t CatalogImpl::getAsset(fetchrequest_ptr_t request) {

  auto result       = std::make_shared<AssetResult>();
  result->_location = request->location;
  Timer overall_timer;
  overall_timer.Start();

  // Check local manifest first for extracted cache
  if (!request->disable_cache) {
    // Parse asset ID to get namespace and name
    auto [namespace_id, asset_name] = parseAssetId(request->asset_id);
    
    // Check for local manifest
    file::Path manifest_path = _catalog->getCacheDir() / "local_manifests" / namespace_id / (asset_name + ".json");
    
    if (manifest_path.doesPathExist()) {
      // Load manifest
      std::string manifest_data;
      FILE* fp = fopen(manifest_path.c_str(), "r");
      if (fp) {
        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        manifest_data.resize(size);
        fread(&manifest_data[0], 1, size, fp);
        fclose(fp);
      }
      
      try {
        auto manifest = nlohmann::json::parse(manifest_data);
        
        // Check if we have the encrypted file locally
        if (manifest.contains("storage_hash")) {
          std::string storage_hash = manifest["storage_hash"].get<std::string>();
          file::Path encrypted_path = _catalog->getCacheDir() / "enc" / (storage_hash + ".enc");
          
          if (encrypted_path.doesPathExist()) {
            // We have the encrypted file locally - no need to download
            logchan_catalog->log("Found local encrypted file: %s", encrypted_path.c_str());
            
            // Skip download phase and go directly to processing
            // Read the encrypted file
            auto raw_data = readCachedFile(encrypted_path);
            if (raw_data) {
              // Process phase (decrypt/decompress)
              Timer process_timer;
              process_timer.Start();
              
              auto processed_data = processAssetData(raw_data, request);
              if (!processed_data) {
                logchan_catalog->log("[DEBUG CatalogImpl] Process phase FAILED");
                if (request->decrypt && manifest.contains("type") && manifest["type"] == "asset_pak") {
                  result->_status = AssetStatus::DECRYPT_FAILED;
                  result->_error_detail = "Failed to decrypt asset";
                }
                return result;
              }
              
              result->_processing_time = process_timer.SecsSinceStart();
              result->_bytes_downloaded = 0; // From local cache
              
              // Handle as asset pak
              handleAssetPak(processed_data, *result, request);
              
              // Check if auto-unwrap single file
              if (manifest.contains("auto_unwrap") && manifest["auto_unwrap"].get<bool>() 
                  && manifest.contains("unwrapped_file")) {
                logchan_catalog->log("Served from local manifest (auto-unwrapped): %s", 
                                   manifest["unwrapped_file"].get<std::string>().c_str());
              }
              
              return result;
            }
          }
        }
      } catch (const std::exception& e) {
        logchan_catalog->log("Failed to parse local manifest: %s", e.what());
      }
    }
  }

  // Check if we already have the assembled encrypted file locally (for chunked assets)
  datablock_ptr_t raw_data;
  if (!request->disable_cache && request->location._chunk_manifest) {
    // Extract storage hash from relative path
    std::string storage_hash = request->location._relative_path;
    if (storage_hash.ends_with(".enc")) {
      storage_hash = storage_hash.substr(0, storage_hash.length() - 4);
    }
    
    // Check if assembled encrypted file exists
    file::Path assembled_path = _catalog->getCacheDir() / "enc" / (storage_hash + ".enc");
    if (assembled_path.doesPathExist()) {
      logchan_catalog->log("Found assembled encrypted file locally: %s", assembled_path.c_str());
      raw_data = readCachedFile(assembled_path);
      result->_bytes_downloaded = 0; // From local cache
    }
  }
  
  // If not found locally, proceed with download
  if (!raw_data) {
    // 1. Download phase
    Timer _download_timer;
    _download_timer.Start();

    raw_data = downloadAssetData(request);
    if (!raw_data) {
      logchan_catalog->log("[DEBUG CatalogImpl] Download phase FAILED");
      result->_status       = AssetStatus::DOWNLOAD_FAILED;
      result->_error_detail = "Failed to download asset _data";
      return result;
    }

    result->_download_time    = _download_timer.SecsSinceStart();
    result->_bytes_downloaded = raw_data->length();
  }

  // 2. Process phase
  Timer process_timer;
  process_timer.Start();

  auto processed_data = processAssetData(raw_data, request);
  if (!processed_data) {
    logchan_catalog->log("[DEBUG CatalogImpl] Process phase FAILED");
    // processAssetData doesn't set _status, so set it here
    if (request->decrypt && request->location._is_encrypted) {
      result->_status       = AssetStatus::DECRYPT_FAILED;
      result->_error_detail = "Failed to decrypt asset";
    } else if (request->location._is_compressed) {
      result->_status       = AssetStatus::DECOMPRESS_FAILED;
      result->_error_detail = "Failed to decompress asset";
    }
    return result;
  }

  result->_processing_time = process_timer.SecsSinceStart();

  // 3. Handle as asset pak (everything is a pak now)
  handleAssetPak(processed_data, *result, request);

  return result;
}
////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::downloadAssetData(fetchrequest_ptr_t request) {
  if (request->location._chunk_manifest) {
    logchan_catalog->log("DEBUG: Using downloadChunkedData for %s", request->location._relative_path.c_str());
    return downloadChunkedData(request);
  } else {
    logchan_catalog->log("DEBUG: Using downloadSingleData for %s", request->location._relative_path.c_str());
    return downloadSingleData(request);
  }
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::downloadSingleData(fetchrequest_ptr_t request) {
  const auto& location = request->location;
  
  // Get cache path for this asset
  file::Path cache_path = getCachePathForAsset(location);

  // Extract storage hash from relative path for verification
  std::string storage_hash = location._relative_path;
  if (storage_hash.size() > 4 && storage_hash.substr(storage_hash.size() - 4) == ".enc") {
    storage_hash = storage_hash.substr(0, storage_hash.size() - 4);
  }

  // Check if cached file exists and is valid (skip if cache disabled)
  if (!request->disable_cache && cache_path.doesPathExist()) {
    if (verifyCachedFileHash(cache_path, storage_hash)) {
      // Cache hit with valid hash
      logchan_catalog->log("Cache hit (verified): %s", storage_hash.c_str());
      auto cached_data = readCachedFile(cache_path);
      if (cached_data) {
        // Update statistics for cache hit
        _stats.atomicOp([&](Stats& stats) {
          stats.cache_hits++;
          stats.bytes_served_from_cache += cached_data->length();
        });
        return cached_data;
      }
    } else {
      // Corrupted cache - delete it
      logchan_catalog->log("Cache corrupted, removing: %s", cache_path.c_str());
      std::remove(cache_path.c_str());
    }
  }

  // Cache miss - download from remote
  // Create a temporary AssetEntry for URL generation
  AssetEntry temp_entry;
  temp_entry._storage_hash = storage_hash;
  temp_entry._namespace    = location._namespace_id;

  URL url = _catalog->getAssetDownloadURL(&temp_entry, location._location_info);

  auto data = downloadFile(url, location._location_info);

  if (data) {
    // Verify downloaded data before caching
    CMD5 hasher;
    hasher.update(data->data(), data->length());
    hasher.finalize();
    std::string computed_hash = hasher.Result().hex_digest();

    if (computed_hash != storage_hash) {
      logchan_catalog->log(
          "ERROR: Downloaded file hash mismatch! Expected %s, got %s", storage_hash.c_str(), computed_hash.c_str());
      return nullptr; // Don't cache or use corrupt data
    }

    // Save verified data to cache
    if (saveToCacheFile(data, cache_path)) {
      logchan_catalog->log("Cached asset: %s", storage_hash.c_str());
    }
  }

  return data;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::downloadChunkedData(fetchrequest_ptr_t request) {
  const auto& location = request->location;
  
  if (!location._chunk_manifest) {
    logchan_catalog->log("ERROR: No chunk manifest for chunked download");
    return nullptr;
  }

  // Check if all chunks are cached and valid (skip cache check if disabled)
  std::vector<datablock_ptr_t> chunks;
  bool all_chunks_cached    = !request->disable_cache; // If cache disabled, force download
  size_t total_cached_bytes = 0;

  if (!request->disable_cache) {
    for (size_t i = 0; i < location._chunk_manifest->_chunks.size(); ++i) {
      file::Path chunk_cache_path = getCachePathForChunk(location, i);

      if (chunk_cache_path.doesPathExist() && verifyCachedChunkHash(chunk_cache_path, location._chunk_manifest->_chunks[i]._hash)) {
        // Chunk is cached and valid
        auto cached_chunk = readCachedFile(chunk_cache_path);
        if (cached_chunk) {
          chunks.push_back(cached_chunk);
          total_cached_bytes += cached_chunk->length();
          continue;
        }
      }

      // Chunk missing or corrupted - need to download all
      all_chunks_cached = false;
      break;
    }
  }

  if (all_chunks_cached) {
    // All chunks were cached and valid
    logchan_catalog->log("Cache hit for all %zu chunks", chunks.size());

    // Update statistics
    _stats.atomicOp([&](Stats& stats) {
      stats.cache_hits++;
      stats.bytes_served_from_cache += total_cached_bytes;
    });

    // Assemble chunks
    ChunkAssembler::Config assembler_config;
    ChunkAssembler assembler(location._chunk_manifest, nullptr, assembler_config);
    auto result = assembler.assembleFromChunks(chunks);

    if (!result->success) {
      logchan_catalog->log("ERROR: Chunk assembly failed: %s", result->error_message.c_str());
      return nullptr;
    }

    return result->assembled_data;
  }

  // Need to download all chunks (all-or-nothing approach)
  logchan_catalog->log("Cache miss or partial cache - downloading all %zu chunks", location._chunk_manifest->_chunks.size());

  // Use shared_ptr for chunks to avoid use-after-free in async callbacks
  auto chunks_ptr = std::make_shared<std::vector<datablock_ptr_t>>();
  chunks_ptr->resize(location._chunk_manifest->_chunks.size());

  // Create a temporary AssetEntry for URL generation
  AssetEntry temp_entry;
  // Extract storage hash from relative_path (format: {storage_hash}.enc)
  std::string storage_hash = location._relative_path;
  if (storage_hash.ends_with(".enc")) {
    storage_hash = storage_hash.substr(0, storage_hash.length() - 4);
  }
  temp_entry._storage_hash = storage_hash;
  temp_entry._namespace    = location._namespace_id;

  // Create download group for parallel chunk downloads
  auto download_group = std::make_shared<DownloadGroup>();
  std::vector<file::Path> chunk_cache_paths;
  
  // Create download tasks for all chunks
  for (size_t i = 0; i < location._chunk_manifest->_chunks.size(); ++i) {
    URL chunk_url = _catalog->getChunkDownloadURL(&temp_entry, i, location._location_info);
    file::Path chunk_cache_path = getCachePathForChunk(location, i);
    chunk_cache_paths.push_back(chunk_cache_path);
    
    // Create a unique temporary file for each chunk download
    file::Path temp_path = file::Path(FormatString("%s.%04zu.tmp", chunk_cache_path.c_str(), i));
    
    auto dl = std::make_shared<Download>(chunk_url, temp_path);
    
    // Set the expected size from the chunk manifest
    dl->_total_bytes = location._chunk_manifest->_chunks[i]._size;
    
    // Add headers if needed (API key authentication)
    if (location._location_info && location._location_info->_api_key_read) {
      std::string api_key = location._location_info->_api_key_read.value();
      dl->setHeader("X-API-Key", api_key);
    }
    if (location._location_info) {
      dl->_ignore_tls_errors = location._location_info->_disable_cert_check;
    } else {
      dl->_ignore_tls_errors = true;
    }
    
    // Capture chunk index for verification
    size_t chunk_idx = i;
    chunk_hash_t expected_hash = location._chunk_manifest->_chunks[i]._hash;
    
    dl->_on_complete._item = [this, chunk_idx, expected_hash, temp_path, chunk_cache_path, chunks_ptr](bool success, const file::Path& path) {
      if (success) {
        // Read downloaded chunk
        auto chunk_data = readCachedFile(temp_path);
        if (!chunk_data) {
          logchan_catalog->log("ERROR: Failed to read downloaded chunk %zu", chunk_idx);
          return;
        }
        
        // Verify chunk hash
        auto xxhasher = std::make_shared<XXH64HASH>();
        xxhasher->init();
        xxhasher->accumulate(chunk_data->data(), chunk_data->length());
        xxhasher->finish();
        chunk_hash_t computed_hash = xxhasher->result();
        
        if (computed_hash != expected_hash) {
          logchan_catalog->log("ERROR: Downloaded chunk %zu hash mismatch", chunk_idx);
          std::remove(temp_path.c_str());
          return;
        }
        
        // Move to cache location
        if (std::rename(temp_path.c_str(), chunk_cache_path.c_str()) == 0) {
          (*chunks_ptr)[chunk_idx] = chunk_data;
        } else {
          // Fallback: copy the data
          saveToCacheFile(chunk_data, chunk_cache_path);
          (*chunks_ptr)[chunk_idx] = chunk_data;
          std::remove(temp_path.c_str());
        }
      }
    };
    
    download_group->addDownload(dl);
  }
  
  // Download all chunks in parallel
  _download_manager->downloadGroup(download_group);
  
  // Wait for completion
  while (!download_group->isComplete()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  // Check if all downloads succeeded
  if (!download_group->allSuccessful()) {
    logchan_catalog->log("ERROR: Failed to download all chunks");
    return nullptr;
  }
  
  // Verify all chunks are present
  for (size_t i = 0; i < chunks_ptr->size(); ++i) {
    if (!(*chunks_ptr)[i]) {
      logchan_catalog->log("ERROR: Missing chunk %zu after download", i);
      return nullptr;
    }
  }

  // Assemble chunks
  ChunkAssembler::Config assembler_config;
  ChunkAssembler assembler(location._chunk_manifest, nullptr, assembler_config);
  auto result = assembler.assembleFromChunks(*chunks_ptr);

  if (!result->success) {
    logchan_catalog->log("ERROR: Chunk assembly failed: %s", result->error_message.c_str());
    return nullptr;
  }

  return result->assembled_data;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::processAssetData(datablock_ptr_t _data, fetchrequest_ptr_t request) {
  const auto& location = request->location;
  auto result = _data;

  // Decrypt if needed
  if (request->decrypt && location._is_encrypted) {
    result = decryptData(result, location._namespace_id);
    if (!result) {
      printf("[ERROR] Decryption failed\n");
      return nullptr;
    }
  }

  // Decompress if needed
  if (location._is_compressed) {
    result = decompressData(result, location._compression_type);
    if (!result) {
      printf("[ERROR] Decompression failed\n");
      return nullptr;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::decryptData(datablock_ptr_t _data, const namespaceid_t& namespace_id) {

  auto codec = _catalog->codecForNamespace(namespace_id);
  if (!codec) {
    printf("[ERROR] No codec available for namespace: %s\n", namespace_id.c_str());
    logchan_catalog->log("ERROR: No codec available for namespace: %s", namespace_id.c_str());
    return nullptr;
  }

  auto result = codec->decrypt(_data.get());
  if (!result) {
    printf("[ERROR] Decryption failed for namespace: %s\n", namespace_id.c_str());
  }
  return result;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::decompressData(datablock_ptr_t _data, CompressionType compression_type) {

  return _data->decompressed();
}

////////////////////////////////////////////////////////////////

void CatalogImpl::handleAssetPak(datablock_ptr_t _data, AssetResult& result, fetchrequest_ptr_t request) {

  // Extract tar contents
  auto archive = util::TarArchive::loadFromMemory(_data);
  if (!archive || !archive->isValid()) {
    result._status       = AssetStatus::DECOMPRESS_FAILED;
    result._error_detail = "Failed to parse tar archive";
    return;
  }

  // Extract all entries to memory
  util::TarExtractOptions extract_options;
  auto extracted_entries = archive->extractToMemory(extract_options);
  if (extracted_entries.empty()) {
    result._status       = AssetStatus::DECOMPRESS_FAILED;
    result._error_detail = "No entries found in tar archive";
    return;
  }

  // AUTO-UNWRAP: If single file, return it directly
  if (extracted_entries.size() == 1) {
    auto& [filename, entry] = *extracted_entries.begin();
    if (entry && entry->data) {
      
      // Set the data directly (auto-unwrap)
      result._data = entry->data;
      result._status = AssetStatus::OK;
      
      // Create local manifest for future cache hits
      if (!request->disable_cache) {
        auto [namespace_id, asset_name] = parseAssetId(request->asset_id);
        
        // Save extracted file to cache
        file::Path extracted_dir = _catalog->getCacheDir() / "extracted" / namespace_id / asset_name;
        extracted_dir.ensureDirectoryExists();
        file::Path extracted_file = extracted_dir / filename;
        saveToCacheFile(entry->data, extracted_file);
        
        // Write to local location if configured
        if (!request->asset_info->_local_loc.empty()) {
          file::Path local_path = request->asset_info->getResolvedLocalPath();
          if (!local_path.empty()) {
            local_path.ensureDirectoryExists();
            file::Path local_file = local_path / filename;
            saveToCacheFile(entry->data, local_file);
            logchan_catalog->log("Written to local: %s", local_file.c_str());
          }
        }
        
        // Create manifest in local_manifests
        nlohmann::json manifest;
        manifest["type"] = "single_file_pak";
        manifest["extracted_at"] = std::time(nullptr);
        manifest["filename"] = filename;
        manifest["storage_hash"] = request->asset_info->_storage_hash;
        manifest["auto_unwrapped"] = true;
        manifest["extracted_path"] = FormatString("extracted/%s/%s/%s", 
                                                  namespace_id.c_str(), 
                                                  asset_name.c_str(), 
                                                  filename.c_str());
        
        file::Path manifest_dir = _catalog->getCacheDir() / "local_manifests" / namespace_id;
        manifest_dir.ensureDirectoryExists();
        file::Path manifest_path = manifest_dir / (asset_name + ".json");
        
        FILE* fp = fopen(manifest_path.c_str(), "w");
        if (fp) {
          std::string json_str = manifest.dump(2);
          fwrite(json_str.c_str(), 1, json_str.length(), fp);
          fclose(fp);
          logchan_catalog->log("Created local manifest: %s", manifest_path.c_str());
        }
      }
      
      logchan_catalog->log("Auto-unwrapped single-file pak: %s (%zu bytes)", 
                          filename.c_str(), entry->data->length());
      return;
    }
  }

  // Multiple files - return as pak_contents
  for (const auto& [filename, entry] : extracted_entries) {
    if (entry && entry->data) {
      result._pak_contents[filename] = entry->data;
    }
  }
  
  // Write multi-file pak to local if configured
  if (!request->asset_info->_local_loc.empty()) {
    writeAssetPakToLocal(request->asset_info, result);
  }
  
  result._status = AssetStatus::OK;
  logchan_catalog->log("Asset pak extraction complete: %zu files", result._pak_contents.size());
}

////////////////////////////////////////////////////////////////

void CatalogImpl::writeAssetPakToLocal(const assetentry_ptr_t& asset_info, AssetResult& result) {
  printf("[DEBUG] Writing asset pak to local location: %s\n", asset_info->_local_loc.c_str());

  // Resolve local path
  file::Path local_path = asset_info->getResolvedLocalPath();
  if (local_path.empty()) {
    printf("[ERROR] Failed to resolve local path\n");
    return;
  }

  // For asset_pak, extract directly to local_path
  // tar_root specifies the source directory structure within the TAR
  file::Path extract_dir = local_path;
  printf("[DEBUG] Extracting to directory: %s\n", extract_dir.c_str());

  // Ensure directory exists
  extract_dir.ensureDirectoryExists();

  // Write each file from _pak_contents
  for (const auto& [filename, _data] : result._pak_contents) {
    if (!_data)
      continue;

    file::Path file_path = extract_dir / filename;
    printf("[DEBUG] Writing file: %s (%zu bytes)\n", file_path.c_str(), _data->length());

    // Ensure parent directory exists
    namespace fs             = boost::filesystem;
    fs::path boost_file_path = file_path.toBFS();
    fs::create_directories(boost_file_path.parent_path());

    // Write file using stdio
    FILE* fp = fopen(file_path.c_str(), "wb");
    if (fp) {
      size_t written = fwrite(_data->data(), 1, _data->length(), fp);
      fclose(fp);
      if (written != _data->length()) {
        printf("[ERROR] Failed to write complete file: %s\n", file_path.c_str());
      }
    } else {
      printf("[ERROR] Failed to open file for writing: %s\n", file_path.c_str());
    }
  }

  printf("[DEBUG] Asset pak extraction to local complete\n");
}

} //namespace ork::asset::catalog {
