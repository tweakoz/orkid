////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/request.h>
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

  file::Path CatalogImpl::localManifestPathForFqid(assetfqid_ptr_t fqid) const {
    return _catalog->getCacheDir()        //
           / "local_manifests"            //
           / fqid->_namespace_id          //
           / (fqid->_asset_id + ".json");
  }

  ////////////////////////////////////////////////////////////////

bool CatalogImpl::getAsset(fetchrequest_ptr_t request) {

  request->_state = AssetState::ENQUEUE_PENDING;
  Timer overall_timer;
  overall_timer.Start();
  auto FQID = request->_fqid;

  ///////////////////////////////////////////////////

  auto download_asset = [&](fetchrequest_ptr_t request) -> datablock_ptr_t {
    Timer _download_timer;
    _download_timer.Start();
    auto raw_data = _downloadAssetData(request);
    request->_download_time    = _download_timer.SecsSinceStart();
    request->_bytes_downloaded = raw_data->length();
    return raw_data;
  };

  ///////////////////////////////////////////////////
  // Check local manifest first for extracted cache
  ///////////////////////////////////////////////////

  // Check for local manifest
  file::Path manifest_path = localManifestPathForFqid(FQID);
   auto local_manifest = _loadLocalManifest(manifest_path);

  ///////////////////////////////////////////////////

  datablock_ptr_t enc_data;
  datablock_ptr_t unw_data;

  ///////////////////////////////////////////////////

  if (local_manifest) { // found local manifest (implying it is downloaded already...)
    enc_data = datablockFromFileAtPath(local_manifest->_encrypted_path);
    if (enc_data) {
      request->_bytes_downloaded = 0; // From local cache
    }
  } // found local manifest

  ///////////////////////////////////////////////////
  // ensure we have the encrypted data
  ///////////////////////////////////////////////////

  if (enc_data == nullptr) {
    enc_data = download_asset(request);
  }
  if( enc_data == nullptr ) {
    logchan_catalog->log("[DEBUG CatalogImpl] Download phase FAILED");
    return false;
  }

  ///////////////////////////////////////////////////
  // ensure we have the decrypted and uncompressed data
  ///////////////////////////////////////////////////

  Timer process_timer;
  process_timer.Start();
  
  unw_data = _processAssetData(enc_data, request);
  if (unw_data == nullptr) {
    logchan_catalog->log("[DEBUG CatalogImpl] Process phase FAILED");
    return false;
  }
  
  request->_processing_time = process_timer.SecsSinceStart();

  ///////////////////////////////////////////////////
  bool unpacked = _extractAssetPak(unw_data, request);  
  ///////////////////////////////////////////////////
  // Create local manifest 
  ///////////////////////////////////////////////////

  if(nullptr==local_manifest){
    auto timestamp = std::time(nullptr);
    auto timestr = std::asctime(std::localtime(&timestamp));
    timestr[strlen(timestr)-1] = 0; // remove newline

    auto local_manifest = std::make_shared<LocalManifest>();
    std::string storage_hash = "???"; // hash of encrypted data
    std::string content_hash = "???"; // hash of unwrapped data
    local_manifest->_fqid = FQID->_original_fqid;
    local_manifest->_storage_hash = storage_hash;
    local_manifest->_content_hash = content_hash;
    local_manifest->_type = FQID->_asset_info->_type;
    local_manifest->_archive_size = unw_data->length();
    local_manifest->_compressed_size = 0;
    local_manifest->_encrypted_size = enc_data->length();
    local_manifest->_timestamp = timestr;
    local_manifest->_auto_unwrap = false;
    local_manifest->_unwrapped_path = ""; // path inside pak if auto_unwrap
    local_manifest->_encrypted_path = _catalog->getCacheDir() / "enc" / (storage_hash + ".enc");
    _saveLocalManifest(local_manifest, manifest_path);
  }

  return unpacked;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::_downloadAssetData(fetchrequest_ptr_t request) {

  auto location = request->_fqid->_location;
  
  OrkAssert(location->_chunk_manifest);

  // Check if all chunks are cached and valid (skip cache check if disabled)
  datablock_list_t chunks;
  bool all_chunks_cached    = false; 
  size_t total_cached_bytes = 0;

  for (size_t i = 0; i < location->_chunk_manifest->_chunks.size(); ++i) {
    file::Path chunk_cache_path = getCachePathForChunk(location, i);

    if (chunk_cache_path.doesPathExist() && verifyCachedChunkHash(chunk_cache_path, location->_chunk_manifest->_chunks[i]._hash)) {
      // Chunk is cached and valid
      auto cached_chunk = datablockFromFileAtPath(chunk_cache_path);
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
    ChunkAssembler assembler(location->_chunk_manifest, nullptr, assembler_config);
    auto result = assembler.assembleFromChunks(chunks);

    if (!result->success) {
      logchan_catalog->log("ERROR: Chunk assembly failed: %s", result->error_message.c_str());
      return nullptr;
    }

    return result->assembled_data;
  }

  // Need to download all chunks (all-or-nothing approach)
  logchan_catalog->log("Cache miss or partial cache - downloading all %zu chunks", location->_chunk_manifest->_chunks.size());

  // Use shared_ptr for chunks to avoid use-after-free in async callbacks
  auto chunks_ptr = std::make_shared<datablock_list_t>();
  chunks_ptr->resize(location->_chunk_manifest->_chunks.size());

  // Create a temporary AssetEntry for URL generation
  AssetEntry temp_entry;
  // Extract storage hash from relative_path (format: {storage_hash}.enc)
  std::string storage_hash = location->_relative_path;
  if (storage_hash.ends_with(".enc")) {
    storage_hash = storage_hash.substr(0, storage_hash.length() - 4);
  }
  temp_entry._storage_hash = storage_hash;
  temp_entry._namespace    = location->_namespace_id;

  // Create download group for parallel chunk downloads
  auto download_group = std::make_shared<DownloadGroup>();
  std::vector<file::Path> chunk_cache_paths;
  
  // Create download tasks for all chunks
  for (size_t i = 0; i < location->_chunk_manifest->_chunks.size(); ++i) {
    URL chunk_url = _catalog->getChunkDownloadURL(&temp_entry, i, location->_location_info);
    file::Path chunk_cache_path = getCachePathForChunk(location, i);
    chunk_cache_paths.push_back(chunk_cache_path);
    
    // Create a unique temporary file for each chunk download
    file::Path temp_path = file::Path(FormatString("%s.%04zu.tmp", chunk_cache_path.c_str(), i));
    
    auto dl = std::make_shared<Download>(chunk_url, temp_path);
    
    // Set the expected size from the chunk manifest
    dl->_total_bytes = location->_chunk_manifest->_chunks[i]._size;
    
    // Add headers if needed (API key authentication)
    if (location->_location_info && location->_location_info->_api_key_read) {
      std::string api_key = location->_location_info->_api_key_read.value();
      dl->setHeader("X-API-Key", api_key);
    }
    if (location->_location_info) {
      dl->_ignore_tls_errors = location->_location_info->_disable_cert_check;
    } else {
      dl->_ignore_tls_errors = true;
    }
    
    // Capture chunk index for verification
    size_t chunk_idx = i;
    chunk_hash_t expected_hash = location->_chunk_manifest->_chunks[i]._hash;
    
    dl->_on_complete._item = [this, chunk_idx, expected_hash, temp_path, chunk_cache_path, chunks_ptr](bool success, const file::Path& path) {
      if (success) {
        // Read downloaded chunk
        auto chunk_data = datablockFromFileAtPath(temp_path);
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
  ChunkAssembler assembler(location->_chunk_manifest, nullptr, assembler_config);
  auto result = assembler.assembleFromChunks(*chunks_ptr);

  if (!result->success) {
    logchan_catalog->log("ERROR: Chunk assembly failed: %s", result->error_message.c_str());
    return nullptr;
  }

  return result->assembled_data;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::_processAssetData(datablock_ptr_t _data, fetchrequest_ptr_t request) {
  const auto& location = request->_fqid->_location;
  auto result = _data;

  // Decrypt if needed
  result = _decryptData(result, location->_namespace_id);
  if (!result) {
    printf("[ERROR] Decryption failed\n");
    return nullptr;
  }

  // Decompress if needed
  if (location->_is_compressed) {
    result = _decompressData(result, location->_compression_type);
    if (!result) {
      printf("[ERROR] Decompression failed\n");
      return nullptr;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::_decryptData(datablock_ptr_t _data, const namespaceid_t& namespace_id) {

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

datablock_ptr_t CatalogImpl::_decompressData(datablock_ptr_t _data, CompressionType compression_type) {

  return _data->decompressed();
}

////////////////////////////////////////////////////////////////

bool CatalogImpl::_extractAssetPak(datablock_ptr_t _data, fetchrequest_ptr_t request) {

  auto fqid = request->_fqid;
  // Extract tar contents
  auto archive = util::TarArchive::loadFromMemory(_data);
  if (!archive || !archive->isValid()) {
    request->_status       = AssetStatus::DECOMPRESS_FAILED;
    request->_error_detail = "Failed to parse tar archive";
    return false;
  }

  // Extract all entries to memory
  util::TarExtractOptions extract_options;
  auto extracted_entries = archive->extractToMemory(extract_options);
  if (extracted_entries.empty()) {
    request->_status       = AssetStatus::DECOMPRESS_FAILED;
    request->_error_detail = "No entries found in tar archive";
    return false;
  }

  // AUTO-UNWRAP: If single file, return it directly
  if (extracted_entries.size() == 1) {
    auto& [filename, entry] = *extracted_entries.begin();
    if (entry && entry->data) {
      
      // Set the data directly (auto-unwrap)
      request->_data = entry->data;
      request->_status = AssetStatus::OK;
      
      // Create local manifest for future cache hits
      if(0)printf("request->_fqid->_asset_id<%s>\n", request->_fqid->_asset_id.c_str());
      auto [namespace_id, asset_name] = parseAssetId(request->_fqid->_asset_id);
      
      // Save extracted file to cache
      file::Path extracted_dir = _catalog->getCacheDir() / "extracted" / namespace_id / asset_name;
      extracted_dir.ensureDirectoryExists();
      file::Path extracted_file = extracted_dir / filename;
      saveToCacheFile(entry->data, extracted_file);
      
      // Write to local location if configured
      if (!fqid->_asset_info->_local_loc.empty()) {
        file::Path local_path = fqid->_asset_info->getResolvedLocalPath();
        if (!local_path.empty()) {
          local_path.ensureDirectoryExists();
          file::Path local_file = local_path / filename;
          saveToCacheFile(entry->data, local_file);
          logchan_catalog->log("Written to local: %s", local_file.c_str());
        }
      }
      auto local_manifest = std::make_shared<LocalManifest>();
      auto timestamp = std::time(nullptr);
      auto timestr = std::asctime(std::localtime(&timestamp));
      timestr[strlen(timestr)-1] = 0; // remove newline
      local_manifest->_fqid = request->_fqid->_original_fqid;
      local_manifest->_storage_hash = request->_fqid->_asset_info->_storage_hash;
      local_manifest->_content_hash = request->_fqid->_asset_info->_content_hash;
      local_manifest->_type = request->_fqid->_asset_info->_type;
      local_manifest->_archive_size = entry->data->length();
      local_manifest->_timestamp = timestr;
      local_manifest->_auto_unwrap = true;
      local_manifest->_unwrapped_path = filename;
      local_manifest->_encrypted_path = _catalog->getCacheDir() / "enc" / (local_manifest->_storage_hash + ".enc");
      file::Path mani_path = _catalog->getCacheDir() / "local_manifests" / namespace_id / (asset_name + ".json");
      _saveLocalManifest(local_manifest, mani_path);
      return true;
    }
  }

  // Multiple files - return as pak_contents
  for (const auto& [filename, entry] : extracted_entries) {
    if (entry && entry->data) {
      request->_pak_contents[filename] = entry->data;
    }
  }
  
  // Write multi-file pak to local if configured
  if (not fqid->_asset_info->_local_loc.empty()) {
    _writeAssetPakToLocal(request);
  }
  
  request->_status = AssetStatus::OK;
  logchan_catalog->log("Asset pak extraction complete: %zu files", request->_pak_contents.size());
  return true;
}

////////////////////////////////////////////////////////////////

void CatalogImpl::_writeAssetPakToLocal(fetchrequest_ptr_t request) {
  auto fqid = request->_fqid;
  auto asset_info = fqid->_asset_info;
  printf("[DEBUG] Writing asset pak to local location: %s\n", asset_info->_local_loc.c_str());

  // Resolve local path
  file::Path local_path = asset_info->getResolvedLocalPath();
  if (local_path.empty()) {
    printf("[ERROR] Failed to resolve local path\n");
    OrkAssert(false);
  }

  // For asset_pak, extract directly to local_path
  // tar_root specifies the source directory structure within the TAR
  file::Path extract_dir = local_path;
  printf("[DEBUG] Extracting to directory: %s\n", extract_dir.c_str());

  // Ensure directory exists
  extract_dir.ensureDirectoryExists();

  // Write each file from _pak_contents
  for (const auto& [filename, _data] : request->_pak_contents) {
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
