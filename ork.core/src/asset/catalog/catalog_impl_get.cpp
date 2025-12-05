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

  auto download_asset = [=](fetchrequest_ptr_t request) -> datablock_ptr_t {
    request->_state = AssetState::DOWNLOADING;
    Timer _download_timer;
    _download_timer.Start();
    auto raw_data = _downloadAssetData(request);
    if(raw_data){
      request->_bytes_downloaded = raw_data->length();
    }
    else {
      auto name = (request->_fqid && request->_fqid->_original_fqid.size())
                      ? request->_fqid->_original_fqid
                      : "UNKNOWN";
      logchan_catalog->log("[DEBUG CatalogImpl] Download phase <%s> FAILED", name.c_str());
    }
    request->_download_time    = _download_timer.SecsSinceStart();

    if(0)printf("[DEBUG CatalogImpl] DOWNLOADING time<%f> bytes<%zu>\n", request->_download_time, request->_bytes_downloaded.load());
    return raw_data;
  };

  ///////////////////////////////////////////////////
  // Check local manifest first for extracted cache
  ///////////////////////////////////////////////////

  // Check for local manifest
  file::Path manifest_path = localManifestPathForFqid(FQID);
   auto local_manifest = _loadLocalManifest(manifest_path);

   if(0)printf("[DEBUG CatalogImpl] manifest_path<%s>\n", manifest_path.c_str());
  ///////////////////////////////////////////////////

  request->_bytes_downloaded = 0; // From local cache
  constexpr size_t MAX_RETRIES = 4;

  datablock_ptr_t enc_data;
  while( (enc_data == nullptr) and (request->_retry_count < MAX_RETRIES) ) {
    request->_retry_count++;
    enc_data = download_asset(request);
  }

  ///////////////////////////////////////////////////
  // ensure we have the encrypted data
  ///////////////////////////////////////////////////

  if( enc_data == nullptr ) {
    logchan_catalog->log("[DEBUG CatalogImpl] Download phase FAILED");
    request->_state = AssetState::FAILED;
    return false;
  }

  request->_state = AssetState::PROCESSING;
  if(0)printf("[DEBUG CatalogImpl] PROCESSING\n");

  ///////////////////////////////////////////////////
  // ensure we have the decrypted and uncompressed data
  ///////////////////////////////////////////////////

  Timer process_timer;
  process_timer.Start();
  
  datablock_ptr_t unw_data = _processAssetData(enc_data, request);
  if (unw_data == nullptr) {
    logchan_catalog->log("[DEBUG CatalogImpl] Process phase FAILED");
    request->_state = AssetState::FAILED;
    return false;
  }
  
  if(0)printf("[DEBUG CatalogImpl] unw_data<%p> size<%zu>\n", (void*) unw_data.get(), unw_data->length());
  request->_processing_time = process_timer.SecsSinceStart();

  ///////////////////////////////////////////////////
  bool unpacked = _extractAssetPak(unw_data, request);  
  ///////////////////////////////////////////////////
  // Create local manifest 
  ///////////////////////////////////////////////////
  if(0)printf("[DEBUG CatalogImpl] unpacked<%d>\n", int(unpacked));

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
  auto fqid = request->_fqid;
  auto linfo = fqid->_location_info;
  auto ainfo = fqid->_asset_info;
  auto chk_manifest = ainfo->_chunk_manifest;
  OrkAssert(linfo);
  OrkAssert(ainfo);
  OrkAssert(chk_manifest);

  if(0){
    printf("[DEBUG _downloadAssetData] location->_location_info is %s\n", linfo ? "SET" : "NULL");
    printf("[DEBUG _downloadAssetData] location_info->_download_url: %s\n", linfo->_download_url.toString().c_str());
  }

  OrkAssert(chk_manifest);

  /////////////////////////////////////////////////
  // figure out which chunks we already have cached
  //  and which we need to download
  /////////////////////////////////////////////////
  size_t NUM_CHUNKS = chk_manifest->_chunks.size();

  if(0){
    printf("[DEBUG] Asset has %zu chunks\n", NUM_CHUNKS);
    //printf("[DEBUG]  location baseurl<%s>\n", location->_base_url.c_str());
    //printf("[DEBUG]  location relpath<%s>\n", location->_relative_path.c_str());
    //printf("[DEBUG]  location nsid<%s>\n", location->_namespace_id.c_str());
    printf("[DEBUG]  locinfo _download_url<%s>\n", linfo->_download_url.toString().c_str());
  }

  /////////////////////////////////////////////////
  // Per-chunk retry configuration (overridable via env vars)
  /////////////////////////////////////////////////
  size_t MAX_CHUNK_RETRIES = 12;
  size_t INITIAL_RETRY_DELAY_MS = 750;  // Start with 750ms delay

  // Check for environment variable overrides
  const char* max_retries_env = std::getenv("ORKID_CHUNK_MAX_RETRIES");
  if (max_retries_env) {
    size_t val = std::atoi(max_retries_env);
    if (val > 0 && val <= 20) {  // Sanity check: 1-20 retries
      MAX_CHUNK_RETRIES = val;
      logchan_catalog->log("Using ORKID_CHUNK_MAX_RETRIES=%zu", MAX_CHUNK_RETRIES);
    }
  }

  const char* retry_delay_env = std::getenv("ORKID_CHUNK_RETRY_DELAY_MS");
  if (retry_delay_env) {
    size_t val = std::atoi(retry_delay_env);
    if (val >= 100 && val <= 10000) {  // Sanity check: 100ms-10s
      INITIAL_RETRY_DELAY_MS = val;
      logchan_catalog->log("Using ORKID_CHUNK_RETRY_DELAY_MS=%zu", INITIAL_RETRY_DELAY_MS);
    }
  }

  using chunk_map_t = std::map<size_t, datablock_ptr_t>;
  using wrapped_chunk_map_t = LockedResource<chunk_map_t>;

  auto CHUNKS = std::make_shared<wrapped_chunk_map_t>();

  // Track which chunks need downloading
  std::vector<size_t> chunks_to_download;

  /////////////////////////////////////////////////
  // Check cache first, build list of needed chunks
  /////////////////////////////////////////////////

  for (size_t i = 0; i < NUM_CHUNKS; ++i) {
    file::Path chunk_cache_path = getCachePathForChunk(fqid, i);
    datablock_ptr_t chunk_data;
    CHUNKS->atomicOp([&](chunk_map_t& unlocked) {
      unlocked[i] = nullptr;
    });

    auto& chunk_info = chk_manifest->_chunks[i];
    if (chunk_cache_path.doesPathExist() && verifyCachedChunkHash(chunk_cache_path, chunk_info._hash)) {
      chunk_data = datablockFromFileAtPath(chunk_cache_path);
      CHUNKS->atomicOp([=](chunk_map_t& unlocked) {
        unlocked[i] = chunk_data;
      });
    } else {
      chunks_to_download.push_back(i);
    }
  }

  if (chunks_to_download.empty()) {
    logchan_catalog->log("Asset %s: All %zu chunks cached ✓", fqid->_original_fqid.c_str(), NUM_CHUNKS);
  } else {
    logchan_catalog->log("Asset %s: %zu chunks total, %zu cached, %zu to download",
                         fqid->_original_fqid.c_str(), NUM_CHUNKS,
                         NUM_CHUNKS - chunks_to_download.size(), chunks_to_download.size());
  }

  /////////////////////////////////////////////////
  // Download all chunks with retry logic
  // Use SINGLE DownloadGroup to avoid counter confusion
  /////////////////////////////////////////////////

  auto download_group = std::make_shared<DownloadGroup>();

  // Create download tasks for all chunks that need downloading
  for (size_t i : chunks_to_download) {
    file::Path chunk_cache_path = getCachePathForChunk(fqid, i);
    std::string chunk_filename = _catalog->getChunkFilename(ainfo->_storage_hash, i);
    file::Path temp_path = file::Path(FormatString("%s.%04zu.tmp", chunk_cache_path.c_str(), i));
    URL chunk_url = linfo->_download_url / chunk_filename;

    auto dl = std::make_shared<Download>(chunk_url, temp_path);
    dl->_total_bytes = chk_manifest->_chunks[i]._size;

    // Add headers if needed (API key authentication)
    if (linfo && linfo->_api_key_read) {
      std::string api_key = linfo->_api_key_read.value();
      dl->setHeader("X-API-Key", api_key);
    }

    dl->_ignore_tls_errors = linfo ? linfo->_disable_cert_check : true;

    // Capture for retry logic
    size_t chunk_idx = i;
    chunk_hash_t expected_hash = chk_manifest->_chunks[i]._hash;

    dl->_on_complete._item = [this, temp_path, chunk_idx, expected_hash, chunk_cache_path, CHUNKS]
                             (bool success, const file::Path& path) {
      if (success) {
        // Read downloaded chunk
        auto chunk_data = datablockFromFileAtPath(temp_path);
        std::remove(temp_path.c_str());
        if (!chunk_data) {
          logchan_catalog->log("  Chunk %zu: failed to read downloaded file ✗", chunk_idx);
          return;
        }

        // Verify chunk hash
        auto xxhasher = std::make_shared<XXH64HASH>();
        xxhasher->init();
        xxhasher->accumulate(chunk_data->data(), chunk_data->length());
        xxhasher->finish();
        chunk_hash_t computed_hash = xxhasher->result();

        if (computed_hash != expected_hash) {
          logchan_catalog->log("  Chunk %zu: hash mismatch (expected=%llu, got=%llu) ✗",
                              chunk_idx, expected_hash, computed_hash);
          std::remove(temp_path.c_str());
          return;
        }

        CHUNKS->atomicOp([=](chunk_map_t& unlocked) {
          unlocked[chunk_idx] = chunk_data;
        });
        saveToCacheFile(chunk_data, chunk_cache_path);
        logchan_catalog->log("  Chunk %zu: downloaded ✓ (%zu bytes, hash=%llu)",
                            chunk_idx, chunk_data->length(), computed_hash);
      } else {
        logchan_catalog->log("  Chunk %zu: download failed (network error) ✗", chunk_idx);
      }
    };

    download_group->addDownload(dl);
  }

  // Download all chunks in the group
  _download_manager->downloadGroup(download_group);

  // Wait for initial download attempt to complete
  while (!download_group->isComplete()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  /////////////////////////////////////////////////
  // Retry logic: Re-download failed chunks
  /////////////////////////////////////////////////

  for (size_t retry = 1; retry <= MAX_CHUNK_RETRIES; retry++) {
    // Find chunks that still need downloading
    std::vector<size_t> failed_chunks;
    CHUNKS->atomicOp([&](const chunk_map_t& unlocked) {
      for (size_t i : chunks_to_download) {
        if (unlocked.at(i) == nullptr) {
          failed_chunks.push_back(i);
        }
      }
    });

    if (failed_chunks.empty()) {
      break; // All chunks downloaded successfully
    }

    // linear backoff delay
    size_t delay_ms = INITIAL_RETRY_DELAY_MS * retry;
    logchan_catalog->log("  Retry %zu/%zu: %zu chunks failed, waiting %zums...",
                        retry, MAX_CHUNK_RETRIES, failed_chunks.size(), delay_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

    // Create NEW DownloadGroup for retries
    auto retry_group = std::make_shared<DownloadGroup>();

    for (size_t chunk_idx : failed_chunks) {
      logchan_catalog->log("  Chunk %zu: retry %zu/%zu",
                          chunk_idx, retry, MAX_CHUNK_RETRIES);

      file::Path chunk_cache_path = getCachePathForChunk(fqid, chunk_idx);
      std::string chunk_filename = _catalog->getChunkFilename(ainfo->_storage_hash, chunk_idx);
      file::Path temp_path = file::Path(FormatString("%s.%04zu.tmp", chunk_cache_path.c_str(), chunk_idx));
      URL chunk_url = linfo->_download_url / chunk_filename;

      auto dl = std::make_shared<Download>(chunk_url, temp_path);
      dl->_total_bytes = chk_manifest->_chunks[chunk_idx]._size;

      if (linfo && linfo->_api_key_read) {
        dl->setHeader("X-API-Key", linfo->_api_key_read.value());
      }
      dl->_ignore_tls_errors = linfo ? linfo->_disable_cert_check : true;

      chunk_hash_t expected_hash = chk_manifest->_chunks[chunk_idx]._hash;

      dl->_on_complete._item = [this, temp_path, chunk_idx, expected_hash, chunk_cache_path, CHUNKS]
                               (bool success, const file::Path& path) {
        if (success) {
          auto chunk_data = datablockFromFileAtPath(temp_path);
          std::remove(temp_path.c_str());
          if (!chunk_data) {
            logchan_catalog->log("  Chunk %zu: failed to read ✗", chunk_idx);
            return;
          }

          auto xxhasher = std::make_shared<XXH64HASH>();
          xxhasher->init();
          xxhasher->accumulate(chunk_data->data(), chunk_data->length());
          xxhasher->finish();
          chunk_hash_t computed_hash = xxhasher->result();

          if (computed_hash != expected_hash) {
            logchan_catalog->log("  Chunk %zu: hash mismatch ✗", chunk_idx);
            std::remove(temp_path.c_str());
            return;
          }

          CHUNKS->atomicOp([=](chunk_map_t& unlocked) {
            unlocked[chunk_idx] = chunk_data;
          });
          saveToCacheFile(chunk_data, chunk_cache_path);
          logchan_catalog->log("  Chunk %zu: retry succeeded ✓", chunk_idx);
        }
      };

      retry_group->addDownload(dl);
    }

    // Download retry batch
    _download_manager->downloadGroup(retry_group);

    // Wait for retry batch to complete
    while (!retry_group->isComplete()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  //////////////////////////////////////////////
  // Verify all chunks are present
  //////////////////////////////////////////////

  datablock_list_t chunks_array;
  bool all_present = true;
  std::vector<size_t> missing_chunks;

  CHUNKS->atomicOp([&](const chunk_map_t& unlocked) {
    chunks_array.resize(unlocked.size());
    for (auto item : unlocked) {
      size_t i = item.first;
      auto chunks_ptr = item.second;
      if (chunks_ptr == nullptr) {
        missing_chunks.push_back(i);
        all_present = false;
      }
      chunks_array[i] = chunks_ptr;
    }
  });

  if (!all_present) {
    logchan_catalog->log("ERROR: Failed to download %zu chunks after %zu retries:",
                        missing_chunks.size(), MAX_CHUNK_RETRIES);
    for (size_t idx : missing_chunks) {
      logchan_catalog->log("  - Chunk %zu", idx);
    }
    return nullptr;
  }

  logchan_catalog->log("All chunks downloaded successfully for %s", fqid->_original_fqid.c_str());

  //////////////////////////////////////////////
  // Assemble chunks
  //////////////////////////////////////////////

  ChunkAssembler::Config assembler_config;
  ChunkAssembler assembler(chk_manifest, nullptr, assembler_config);
  auto result = assembler.assembleFromChunks(chunks_array);

  if (!result->success) {
    logchan_catalog->log("ERROR: Chunk assembly failed: %s", result->error_message.c_str());
    return nullptr;
  }

  return result->assembled_data;
}

////////////////////////////////////////////////////////////////

datablock_ptr_t CatalogImpl::_processAssetData(datablock_ptr_t _data, fetchrequest_ptr_t request) {
  auto fqid = request->_fqid;
  auto ainfo = fqid->_asset_info;
  auto result = _data;

  // Decrypt if needed
  result = _decryptData(result, fqid->_namespace_id);
  if (!result) {
    printf("[ERROR] Decryption failed\n");
    return nullptr;
  }

  // Decompress if needed
  result = _decompressData(result, ainfo->_compression_type);
  if (!result) {
    printf("[ERROR] Decompression failed\n");
    return nullptr;
  }

  if(0)printf("[DEBUG CatalogImpl] processAssetData complete: decompressed size<%zu>\n", result->length());

  auto cont_hash = request->_fqid->_asset_info->_content_hash;
  auto stor_hash = request->_fqid->_asset_info->_storage_hash;

  // verify storage hash
  if(0){
    CMD5 md5_storage;
    md5_storage.update(_data->data(), _data->length());
    md5_storage.finalize();
    auto computed_stor_hash = md5_storage.Result().hex_digest();  
    printf("[DEBUG CatalogImpl] storage_hash<%s> computed_stor_hash<%s>\n", stor_hash.c_str(), computed_stor_hash.c_str());
  }

  // Verify content hash
  if(0) {
    CMD5 md5_content;
    md5_content.update(result->data(), result->length());
    md5_content.finalize();
    auto computed_hash = md5_content.Result().hex_digest();  
    printf("[DEBUG CatalogImpl] content_hash<%s> computed_hash<%s>\n", cont_hash.c_str(), computed_hash.c_str());
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
  auto expected_hash = fqid->_asset_info->_content_hash;

  // Verify content hash
  CMD5 md5_content;
  md5_content.update(_data->data(), _data->length());
  md5_content.finalize();
  auto computed_hash = md5_content.Result().hex_digest();

  if (computed_hash != expected_hash) {
    logchan_catalog->log("ERROR: Content hash mismatch for %s: expected %s, got %s",
                         fqid->_original_fqid.c_str(), expected_hash.c_str(), computed_hash.c_str());
    request->_status = AssetStatus::CHECKSUM;
    request->_error_detail = "Content hash mismatch: expected " + expected_hash + ", got " + computed_hash;
    return false;
  }

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
  printf("[DEBUG _extractAssetPak] extracted_entries.size()=%zu\n", extracted_entries.size());
  if (extracted_entries.empty()) {
    request->_status       = AssetStatus::DECOMPRESS_FAILED;
    request->_error_detail = "No entries found in tar archive";
    return false;
  }

  // AUTO-UNWRAP: If single file, return it directly
  if (extracted_entries.size() == 1) {
    auto& [filename, entry] = *extracted_entries.begin();
    printf("[DEBUG _extractAssetPak] single file: filename='%s', entry=%p, entry->data=%p, data_len=%zu\n",
           filename.c_str(), (void*)entry.get(), entry ? (void*)entry->data.get() : nullptr,
           (entry && entry->data) ? entry->data->length() : 0);
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
      //saveToCacheFile(entry->data, extracted_file);
      
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
