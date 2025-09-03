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
    request->_state = AssetState::DOWNLOADING;
    Timer _download_timer;
    _download_timer.Start();
    auto raw_data = _downloadAssetData(request);
    request->_download_time    = _download_timer.SecsSinceStart();
    request->_bytes_downloaded = raw_data->length();

    printf("[DEBUG CatalogImpl] DOWNLOADING time<%f> bytes<%zu>\n", request->_download_time, request->_bytes_downloaded.load());
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
    printf("[DEBUG CatalogImpl] re-downloading asset data\n");
    enc_data = download_asset(request);
  }
  if( enc_data == nullptr ) {
    logchan_catalog->log("[DEBUG CatalogImpl] Download phase FAILED");
    request->_state = AssetState::FAILED;
    return false;
  }

  request->_state = AssetState::PROCESSING;
  printf("[DEBUG CatalogImpl] PROCESSING\n");

  ///////////////////////////////////////////////////
  // ensure we have the decrypted and uncompressed data
  ///////////////////////////////////////////////////

  Timer process_timer;
  process_timer.Start();
  
  unw_data = _processAssetData(enc_data, request);
  if (unw_data == nullptr) {
    logchan_catalog->log("[DEBUG CatalogImpl] Process phase FAILED");
    request->_state = AssetState::FAILED;
    return false;
  }
  
  printf("[DEBUG CatalogImpl] unw_data<%p> size<%zu>\n", (void*) unw_data.get(), unw_data->length());
  request->_processing_time = process_timer.SecsSinceStart();

  ///////////////////////////////////////////////////
  bool unpacked = _extractAssetPak(unw_data, request);  
  ///////////////////////////////////////////////////
  // Create local manifest 
  ///////////////////////////////////////////////////
  printf("[DEBUG CatalogImpl] unpacked<%d>\n", int(unpacked));

  if(false==unpacked) {
    printf("[DEBUG CatalogImpl] Unpack phase FAILED: %s\n", request->_error_detail.c_str());
    return false;
  }

  if(nullptr==local_manifest){
    auto timestamp = std::time(nullptr);
    auto timestr = std::to_string(timestamp);

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

  request->_state = AssetState::SUCCEEDED;

  return unpacked;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::_downloadAssetData(fetchrequest_ptr_t request) {

  auto location = request->_fqid->_location;
  
  OrkAssert(location->_chunk_manifest);

  /////////////////////////////////////////////////
  // figure out which chunks we already have cached
  //  and which we need to download
  /////////////////////////////////////////////////
  size_t NUM_CHUNKS = location->_chunk_manifest->_chunks.size();
  printf("[DEBUG] Asset has %zu chunks\n", NUM_CHUNKS);
  ///////////////////////////////////////////////////
  // Create a temporary AssetEntry for URL generation
  ///////////////////////////////////////////////////

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
  
  /////////////////////////////////////////////////
  // Create download tasks for all chunks
  /////////////////////////////////////////////////

  using chunk_map_t = std::map<size_t, datablock_ptr_t>;
  LockedResource<chunk_map_t> CHUNKS;
  std::unordered_map<size_t, file::Path> CHUNKS_PATHS;

  for (size_t i = 0; i < NUM_CHUNKS; ++i) {
    file::Path chunk_cache_path = getCachePathForChunk(location, i);
    CHUNKS_PATHS[i] = chunk_cache_path;
    datablock_ptr_t chunk_data;
    CHUNKS.atomicOp([&](chunk_map_t& unlocked) {
      unlocked[i] = nullptr;
    });
    auto chunk_mani = location->_chunk_manifest;
    auto& chunk_info = chunk_mani->_chunks[i];
    if (chunk_cache_path.doesPathExist() && verifyCachedChunkHash(chunk_cache_path, chunk_info._hash)) {
      chunk_data = datablockFromFileAtPath(chunk_cache_path);
      CHUNKS.atomicOp([&](chunk_map_t& unlocked) {
        unlocked[i] = chunk_data;
      });
    }

    /////////////////////////
    // cached ?
    /////////////////////////

    if(chunk_data) {
      continue;
    }

    /////////////////////////
    // Create a unique temporary file for each chunk download
    /////////////////////////

    file::Path temp_path = file::Path(FormatString("%s.%04zu.tmp", chunk_cache_path.c_str(), i));
    URL chunk_url = _catalog->getChunkDownloadURL(&temp_entry, i, location->_location_info);    
    auto dl = std::make_shared<Download>(chunk_url, temp_path);
     dl->_total_bytes = location->_chunk_manifest->_chunks[i]._size;
    
    //////////////////////////////////////////////
    // Add headers if needed (API key authentication)
    //////////////////////////////////////////////

    if (location->_location_info && location->_location_info->_api_key_read) {
      std::string api_key = location->_location_info->_api_key_read.value();
      dl->setHeader("X-API-Key", api_key);
    }

    dl->_ignore_tls_errors = location->_location_info                      //
                           ? location->_location_info->_disable_cert_check //
                           : true;
    
    //////////////////////////////////////////////
    // on download complete, save to chunk map and cache
    //////////////////////////////////////////////
    
    size_t chunk_idx = i;
    chunk_hash_t expected_hash = chunk_info._hash;
    
    dl->_on_complete._item = [this,                                             //
                              temp_path,                                        //
                              chunk_idx,                                        //
                              expected_hash,                                    //
                              chunk_cache_path,                                 //
                              &CHUNKS](bool success, const file::Path& path) {  //
      if (success) {
        // Read downloaded chunk
        auto chunk_data = datablockFromFileAtPath(temp_path);
        std::remove(temp_path.c_str());
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
        
        CHUNKS.atomicOp([=](chunk_map_t& unlocked) {
          unlocked[chunk_idx] = chunk_data;
        });
        saveToCacheFile(chunk_data, chunk_cache_path);
      }
    };
    
    download_group->addDownload(dl);
  }
  
  //////////////////////////////////////////////
  // Download all chunks in parallel
  //////////////////////////////////////////////

  _download_manager->downloadGroup(download_group);
  
  //////////////////////////////////////////////
  // Wait for completion
  //////////////////////////////////////////////

  while (!download_group->isComplete()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  //////////////////////////////////////////////
  // Check if all downloads succeeded
  //////////////////////////////////////////////

  if (!download_group->allSuccessful()) {
    logchan_catalog->log("ERROR: Failed to download all chunks");
    return nullptr;
  }
  
  //////////////////////////////////////////////
  // Verify all chunks are present
  //////////////////////////////////////////////

  datablock_list_t chunks_array;
  bool all_present = true;
  CHUNKS.atomicOp([&](const chunk_map_t& unlocked) {
    chunks_array.resize(unlocked.size());
    for (auto item : unlocked) {
      size_t i = item.first;
      auto chunks_ptr = item.second;
      if (chunks_ptr == nullptr) {
        logchan_catalog->log("ERROR: Missing chunk %zu after download", i);
        all_present = false;
      }
      chunks_array[i] = chunks_ptr;
    }
  });

  //////////////////////////////////////////////

  if( not all_present ) {
    return nullptr;
  }

  //////////////////////////////////////////////
  // Assemble chunks
  //////////////////////////////////////////////

  ChunkAssembler::Config assembler_config;
  ChunkAssembler assembler(location->_chunk_manifest, nullptr, assembler_config);
  auto result = assembler.assembleFromChunks(chunks_array);

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
  result = _decompressData(result, location->_compression_type);
  if (!result) {
    printf("[ERROR] Decompression failed\n");
    return nullptr;
  }

  printf("[DEBUG CatalogImpl] processAssetData complete: decompressed size<%zu>\n", result->length());
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
      auto timestr = std::to_string(timestamp);
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
