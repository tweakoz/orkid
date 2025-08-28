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

  // 1. Download phase
  Timer _download_timer;
  _download_timer.Start();

  auto raw_data = downloadAssetData(request);
  if (!raw_data) {
    logchan_catalog->log("[DEBUG CatalogImpl] Download phase FAILED");
    result->_status       = AssetStatus::DOWNLOAD_FAILED;
    result->_error_detail = "Failed to download asset _data";
    return result;
  }

  result->_download_time    = _download_timer.SecsSinceStart();
  result->_bytes_downloaded = raw_data->length();

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

  // 3. Handle by type
  if (request->asset_info->_type == "asset_pak") {
    handleAssetPak(processed_data, *result);
    // Also write to local location if available
    if (!request->asset_info->_local_loc.empty()) {
      writeAssetPakToLocal(request->asset_info, *result);
    }
  } else {
    handleRegularAsset(processed_data, *result);
  }

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

void CatalogImpl::handleAssetPak(datablock_ptr_t _data, AssetResult& result) {

  // Extract tar contents
  auto archive = util::TarArchive::loadFromMemory(_data);
  if (!archive || !archive->isValid()) {
    printf("[ERROR] Failed to parse tar archive\n");
    result._status       = AssetStatus::DECOMPRESS_FAILED;
    result._error_detail = "Failed to parse tar archive";
    return;
  }

  // Extract all entries to memory
  util::TarExtractOptions extract_options;
  auto extracted_entries = archive->extractToMemory(extract_options);
  if (extracted_entries.empty()) {
    printf("[ERROR] No entries found in tar archive\n");
    result._status       = AssetStatus::DECOMPRESS_FAILED;
    result._error_detail = "No entries found in tar archive";
    return;
  }

  // Convert tar entries to AssetResult format
  for (const auto& [filename, entry] : extracted_entries) {
    if (entry && entry->data) {
      result._pak_contents[filename] = entry->data;
      printf("[DEBUG] Extracted: %s (%zu bytes)\n", filename.c_str(), entry->data->length());
    }
  }

  result._status = AssetStatus::OK;
  printf("[DEBUG] Asset pak extraction complete: %zu files\n", result._pak_contents.size());
}

////////////////////////////////////////////////////////////////

void CatalogImpl::handleRegularAsset(datablock_ptr_t _data, AssetResult& result) {
  result._data             = _data;
  result._status           = AssetStatus::OK;
  result._bytes_downloaded = _data->length();
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
