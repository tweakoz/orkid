////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/namespace.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/uploader.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/asset/catalog/request.h>
#include <ork/kernel/string/deco.inl>
#include <ork/file/file.h>
#include <ork/object/ObjectClass.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <ork/util/upload.h>
#include <ork/util/logger.h>
#include <ork/util/password_provider.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <nlohmann/json.hpp>
#include <ctime>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <ctime>
#include "catalog_impl.h"

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

// Get current platform
static std::string getCurrentPlatform() {
  #ifdef __APPLE__
    return "darwin";
  #elif __linux__
    return "linux";
  #elif _WIN32
    return "win32";
  #else
    return "unknown";
  #endif
}

// Save chunk manifest to disk
static void saveChunkManifest(chunkmanifest_ptr_t manifest, const file::Path& path) {
  if (!manifest) return;
  
  // Create JSON representation
  rapidjson::Document doc;
  doc.SetObject();
  auto& allocator = doc.GetAllocator();
  
  // Add manifest fields
  doc.AddMember("total_size", rapidjson::Value(static_cast<uint64_t>(manifest->_total_size)), allocator);
  doc.AddMember("file_hash", rapidjson::Value(static_cast<uint64_t>(manifest->_file_hash)), allocator);
  
  // Add chunks array
  rapidjson::Value chunks_array(rapidjson::kArrayType);
  for (const auto& chunk : manifest->_chunks) {
    rapidjson::Value chunk_obj(rapidjson::kObjectType);
    chunk_obj.AddMember("offset", rapidjson::Value(static_cast<uint64_t>(chunk._offset)), allocator);
    chunk_obj.AddMember("size", rapidjson::Value(static_cast<uint64_t>(chunk._size)), allocator);
    chunk_obj.AddMember("hash", rapidjson::Value(static_cast<uint64_t>(chunk._hash)), allocator);
    chunks_array.PushBack(chunk_obj, allocator);
  }
  doc.AddMember("chunks", chunks_array, allocator);
  
  // Write to file
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);
  
  std::ofstream out_file(path.c_str());
  if (out_file.is_open()) {
    out_file << buffer.GetString();
    out_file.close();
  }
}

////////////////////////////////////////////////////////////////////////////////

bool AssetEntry::supportsCurrentPlatform() const {
  // If no platforms specified, support all platforms (backward compatibility)
  if (_platforms.empty()) {
    return true;
  }
  
  // Check if current platform is in the supported list
  std::string current = getCurrentPlatform();
  return std::find(_platforms.begin(), _platforms.end(), current) != _platforms.end();
}

////////////////////////////////////////////////////////////////////////////////

std::string AssetEntry::buildFullyQualifiedId() const {
  // Try to use the namespace pointer to walk the hierarchy
  auto ns_ptr = _namespace_ptr.lock();
  if (ns_ptr) {
    return ns_ptr->buildFullPath() + "|" + _id;
  }
  
  // Fallback to string namespace if pointer is not available
  if (_namespace.empty()) {
    return _id;
  }
  return _namespace + "|" + _id;
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::_archiveAsset() - archive locals to TAR
////////////////////////////////////////////////////////////////////////////////

datablock_ptr_t AssetEntry::_archiveAsset(assetfqid_ptr_t fqid) {
  ////////////////////////////////////////////////////////
  // Check if we have valid file information
  ////////////////////////////////////////////////////////

  if (_local_loc.empty()) {
    return nullptr;
  }
  
  ////////////////////////////////////////////////////////
  // Resolve local path using getResolvedLocalPath()
  ////////////////////////////////////////////////////////

  file::Path base_path = getResolvedLocalPath();
  if (base_path.empty()) {
    return nullptr;
  }
  
  ////////////////////////////////////////////////////////
  // Only asset_pak supported
  ////////////////////////////////////////////////////////

  if (_type != "asset_pak") {
    logchan_catalog->log("ERROR: Only asset_pak type is supported. Asset type: %s", _type.c_str());
    OrkAssert(false);
  }
  
  ////////////////////////////////////////////////////////
  // Determine source directory using tar_root field
  ////////////////////////////////////////////////////////

  file::Path source_dir;
  if (_tar_root.empty()) {
    // No tar_root specified - use base_path directly
    source_dir = base_path;
  } else {
    // Use tar_root to find the source directory
    source_dir = base_path / _tar_root;
  }
  
  ////////////////////////////////////////////////////////
  // Check if directory exists
  ////////////////////////////////////////////////////////

  if (not source_dir.doesPathExist()) {
    logchan_catalog->log("ERROR: Asset pak directory does not exist: %s", source_dir.c_str());
    OrkAssert(false);
  }
  
  ////////////////////////////////////////////////////////
  // Create TAR from directory and store it for later use
  ////////////////////////////////////////////////////////

  auto catalog = getCatalog();
  OrkAssert(catalog!=nullptr);
  auto tar_data = catalog->_packFromLocal(fqid);
  OrkAssert(tar_data);
  return tar_data;
} 
  
////////////////////////////////////////////////////////////////////////////////
// AssetEntry::_encryptAsset() - encrypt with codec
////////////////////////////////////////////////////////////////////////////////

datablock_ptr_t AssetEntry::_encryptAsset(datablock_ptr_t tar_data) {
  encryptioncodec_ptr_t codec;
  auto parent_manifest = _parent_manifest.lock();
  if (parent_manifest) {
    codec = parent_manifest->getCodec();
    printf("[DEBUG REPACKAGE] Got codec from parent manifest for namespace: %s\n", _namespace.c_str());
  }
  OrkAssert(codec);
  return codec->encrypt(tar_data.get());
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::repackage() - Repackage asset (recompute hashes, rechunk if needed)
////////////////////////////////////////////////////////////////////////////////

void AssetEntry::repackage() {
  
  auto catalog = getCatalog();
  auto fqid_str = buildFullyQualifiedId();
  auto fqid = catalog->findAsset(fqid_str); 
  ////////////////////////////////////////////////////////
  // Archive asset to TAR
  ////////////////////////////////////////////////////////

  auto tar_data = _archiveAsset(fqid);
  _archive_size = tar_data->length();
  if(0)printf("[TARX] tar_data out hash<0x%llx>\n", tar_data->hash());
  
  ////////////////////////////////////////////////////////
  // Compute content hash from TAR data
  ////////////////////////////////////////////////////////

  CMD5 content_hasher;
  content_hasher.update(tar_data->data(), tar_data->length());
  content_hasher.finalize();
  Md5Sum content_md5_result = content_hasher.Result();
  _content_hash = content_md5_result.hex_digest();
  if(0)printf("[TARX] _content_hash<%s>\n", _content_hash.c_str());

  ////////////////////////////////////////////////////////
  // compress
  ////////////////////////////////////////////////////////

  auto compressed_data = tar_data->compressed(8); // level 0
  OrkAssert(compressed_data);
  _compressed_size = compressed_data->length();

    
  ////////////////////////////////////////////////////////
  // Encrypt the TAR data
  ////////////////////////////////////////////////////////

  auto encrypted_data = _encryptAsset(compressed_data);
  OrkAssert(encrypted_data);
  _encrypted_size = encrypted_data->length();

  ////////////////////////////////////////////////////////
  // Compute storage hash from encrypted data
  ////////////////////////////////////////////////////////

  CMD5 storage_hasher;
  storage_hasher.update(encrypted_data->data(), encrypted_data->length());
  storage_hasher.finalize();
  Md5Sum storage_md5_result = storage_hasher.Result();
  _storage_hash = storage_md5_result.hex_digest();

  ////////////////////////////////////////////////////////
  // Chunkify..
  ////////////////////////////////////////////////////////

  auto disassembly_result = ChunkDisassembler::disassemble(
    encrypted_data,
    nullptr,  // Already encrypted, don't encrypt again
    CompressionType::NONE  // Already processed
  );
  
  OrkAssert(disassembly_result->_success);
  OrkAssert(disassembly_result->_chunks.size() == disassembly_result->_chunk_manifest->_chunks.size());

  _chunk_manifest = disassembly_result->_chunk_manifest;
  logchan_catalog->log("CHUNK DEBUG: Set _chunk_manifest for %s - %zu chunks, total_size=%zu", 
                      _id.c_str(), _chunk_manifest->_chunks.size(), _chunk_manifest->_total_size);
  
  // Get chunks directory
  OrkAssert(catalog!=nullptr);
  file::Path chunks_dir = catalog->getChunksDir();
  chunks_dir.ensureDirectoryExists();
  
  // Save each chunk with proper hash-based naming
  for (size_t i = 0; i < disassembly_result->_chunks.size(); ++i) {
    const auto& chunk_data = disassembly_result->_chunks[i];
    const auto& chunk_meta = _chunk_manifest->_chunks[i];

    // Chunk filename: {storage_hash}.chunk.{index:04d}
    std::string chunk_filename = catalog->getChunkFilename(_storage_hash, i);
    file::Path chunk_path = chunks_dir / chunk_filename;
    
    // Write chunk to disk
    File chunk_file(chunk_path, EFM_WRITE);
    chunk_file.Write(chunk_data->data(), chunk_data->length());
    
    logchan_catalog->log("Saved chunk %zu/%zu: %s (size: %zu)", 
                        i + 1, disassembly_result->_chunks.size(),
                        chunk_filename.c_str(), chunk_data->length());
    }
    
  // Save chunk manifest
  auto enc_dir = catalog->getEncryptedDir();
  file::Path manifest_path = enc_dir / (_storage_hash + ".chunkmanifest");
  saveChunkManifest(_chunk_manifest, manifest_path);
  logchan_catalog->log("Saved chunk manifest: %s", manifest_path.c_str());
    
  ////////////////////////////////////////////////////////

  logchan_catalog->log("CHUNK DEBUG: End of repackage for %s - _chunk_manifest=%p", 
                       _id.c_str(), _chunk_manifest.get());
    
  // Create local manifest for immediate use without upload
  if (catalog && _type == "asset_pak" && !_storage_hash.empty()) {
    // Create local manifest entry
    auto impl = catalog->_impl.getShared<CatalogImpl>();
    if (impl) {
      auto timestamp = std::time(nullptr);
      auto timestamp_str = std::to_string(timestamp);
      localmanifest_ptr_t local_mani = std::make_shared<LocalManifest>();
      local_mani->_fqid = fqid_str;
      local_mani->_storage_hash = _storage_hash;
      local_mani->_content_hash = _content_hash;
      local_mani->_type = _type;
      local_mani->_archive_size = _archive_size;
      local_mani->_encrypted_size = _encrypted_size;
      local_mani->_compressed_size = _compressed_size;
      local_mani->_timestamp = timestamp_str;
      local_mani->_auto_unwrap = (_filters.size() == 1);
      if (local_mani->_auto_unwrap) {
        local_mani->_unwrapped_path = _filters[0];
      }
      file::Path manifest_path = impl->localManifestPathForFqid(fqid);
      impl->_saveLocalManifest(local_mani, manifest_path);
    }    
  }
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::getResolvedLocalPath() - Get resolved local path
////////////////////////////////////////////////////////////////////////////////
file::Path AssetEntry::getResolvedLocalPath() const {
  if (_local_loc.empty()) {
    return file::Path();
  }
  
  file::Path local_path;
  if (_local_loc.find("<") != std::string::npos) {
    // Template path - need to resolve
    std::string resolved_local = _local_loc;
    
    // Replace known template paths
    if (resolved_local.find("<stage>") == 0) {
      resolved_local.replace(0, 7, file::Path::stage_dir().c_str());
    } else if (resolved_local.find("<assetcache>") == 0) {
      std::string cache_path = (file::Path::stage_dir() / "assetcache").c_str();
      resolved_local.replace(0, 12, cache_path);
    } else if (resolved_local.find("<cache>") == 0) {
      std::string cache_path = (file::Path::stage_dir() / "assetcache").c_str();
      resolved_local.replace(0, 7, cache_path);
    }
    
    local_path = file::Path(resolved_local);
  } else {
    local_path = file::Path(_local_loc);
  }
  
  return local_path;
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::getLocalEncryptedPath() - Get path to local encrypted file
////////////////////////////////////////////////////////////////////////////////
file::Path AssetEntry::getLocalEncryptedPath() const {
  // Check if we have a storage hash
  if (_storage_hash.empty()) {
    return file::Path(); // Return empty path if not packaged yet
  }
  
  // Get catalog from parent manifest to access cache directory
  file::Path enc_dir;
  auto parent_manifest = _parent_manifest.lock();
  if (parent_manifest) {
    auto catalog = parent_manifest->getParentCatalog();
    if (catalog) {
      enc_dir = catalog->getEncryptedDir();
    } else {
      // Fallback to default location
      enc_dir = file::Path::stage_dir() / "assetcache" / "enc";
    }
  } else {
    // Fallback to default location  
    enc_dir = file::Path::stage_dir() / "assetcache" / "enc";
  }
  return enc_dir / (_storage_hash + ".enc");
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::isRepackaged() - Check if asset has been repackaged
////////////////////////////////////////////////////////////////////////////////

bool AssetEntry::isRepackaged() const {
  // Asset is repackaged if it has a storage hash
  return !_storage_hash.empty();
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::upload() - Upload asset to destination
////////////////////////////////////////////////////////////////////////////////

uploadreceipt_ptr_t AssetEntry::upload(
    const AssetConfig& config,
    locationinfo_ptr_t location_info,
    chunk_completed_callback_t on_chunk_completed) const {
  
  ////////////////////////////////////
  // Verify entry is repackaged
  ////////////////////////////////////

  if (!isRepackaged()) {
    logchan_catalog->log("ERROR: Asset not repackaged");
    throw std::runtime_error("Asset must be repackaged before upload");
  }
  
  ////////////////////////////////////
  // Get API key from namespace encryption key if not set in location
  ////////////////////////////////////

  if (!location_info->_api_key_write.has_value()) {
    std::string encryption_key = config.getEncryptionKeyForNamespace(_namespace);
    if (!encryption_key.empty()) {
      location_info->_api_key_write = encryption_key;
      // Using namespace key as API key
    }
  }
  
  ////////////////////////////////////
  // Check if password authentication is required for uploads
  ////////////////////////////////////

  if (location_info->_api_key_write.has_value()) {
    std::string api_key = location_info->_api_key_write.value();
    if (PasswordProvider::requiresPasswordAuth(api_key)) {
      std::string host = location_info->_upload_url._host;
      std::string prompt = FormatString("Upload password for %s: ", host.c_str());
      auto password = PasswordProvider::getPassword(prompt, true); // Allow caching
      
      if (password.has_value()) {
        location_info->_api_key_write = password.value();
        logchan_catalog->log("Using password authentication for upload to: %s", host.c_str());
      } else {
        logchan_catalog->log("ERROR: Password authentication required but not provided for upload");
        auto receipt = std::make_shared<UploadReceipt>();
        receipt->success = false;
        receipt->status_message = "Password authentication required but not provided";
        return receipt;
      }
    }
  }
  
  ////////////////////////////////////
  // Create upload receipt
  ////////////////////////////////////

  auto receipt = std::make_shared<UploadReceipt>();
  receipt->upload_id = _id;  // Use asset id as upload id
  receipt->namespace_id = _namespace;
  receipt->destination = location_info->_upload_url.toString();
  receipt->timestamp = time(nullptr);

  ////////////////////////////////////
  // Getting parent manifest and catalog
  ////////////////////////////////////

  auto asset_manifest = getParentManifest();
  OrkAssert(asset_manifest);

  auto catalog = asset_manifest->getParentCatalog();
  if (!catalog) {
    logchan_catalog->log("ERROR: Asset manifest has no parent catalog");
    throw std::runtime_error("Asset manifest has no parent catalog");
  }

  OrkAssert(_chunk_manifest);
  
  ////////////////////////////////////          
  // Store chunk manifest in receipt for transparency
  ////////////////////////////////////          

  receipt->_chunk_manifest = _chunk_manifest;
  
  ////////////////////////////////////
  // Upload chunks using batch upload for concurrency
  ////////////////////////////////////

  logchan_catalog->log("Starting upload of chunked asset: %zu total chunks", _chunk_manifest->_chunks.size());

  ////////////////////////////////////
  // Phase 1: Identify locally available chunks
  ////////////////////////////////////

  std::vector<size_t> available_chunk_indices;
  std::vector<size_t> missing_local_indices;

  for (size_t chunk_idx = 0; chunk_idx < _chunk_manifest->_chunks.size(); ++chunk_idx) {
    std::string chunk_filename = catalog->getChunkFilename(_storage_hash, chunk_idx);
    file::Path chunk_path = catalog->getChunksDir() / chunk_filename;

    if (chunk_path.doesPathExist()) {
      available_chunk_indices.push_back(chunk_idx);
    } else {
      missing_local_indices.push_back(chunk_idx);
    }
  }

  logchan_catalog->log("  Local status: %zu available, %zu missing",
                       available_chunk_indices.size(), missing_local_indices.size());

  if (available_chunk_indices.empty()) {
    logchan_catalog->log("ERROR: No chunks available locally for upload");
    receipt->success = false;
    receipt->status_message = "No chunks available locally";
    return receipt;
  }

  ////////////////////////////////////
  // Phase 2: Verify which chunks are already on server
  ////////////////////////////////////

  chunkverifyrequest_vect_t verify_requests;
  for (size_t chunk_idx : available_chunk_indices) {
    ChunkVerifyRequest req;
    req.filename = catalog->getChunkFilename(_storage_hash, chunk_idx);
    req.expected_hash = _chunk_manifest->_chunks[chunk_idx]._hash;
    verify_requests.push_back(req);
  }

  // Build verify URL: /api/{endpoint}/verify
  // Extract endpoint from upload URL pattern: /{endpoint}/upload/...
  std::string upload_path = location_info->_upload_url._path;
  std::regex endpoint_regex("^/([^/]+)/upload");
  std::smatch match;
  std::string endpoint = "std"; // Default
  if (std::regex_search(upload_path, match, endpoint_regex)) {
    endpoint = match[1];
  }

  URL verify_url = location_info->_upload_url;
  verify_url._path = "/api/" + endpoint + "/verify";

  // Setup headers for verification
  std::map<std::string, std::string> verify_headers;
  if (location_info->_api_key_read.has_value()) {
    verify_headers["X-API-Key"] = location_info->_api_key_read.value();
  } else if (location_info->_api_key_write.has_value()) {
    // Fallback to write key if read key not available
    verify_headers["X-API-Key"] = location_info->_api_key_write.value();
  }

  // Get download manager from catalog
  auto impl = catalog->_impl.getShared<CatalogImpl>();
  auto download_mgr = impl->_download_manager;

  // Call verification
  chunkverifyresult_vect_t verify_results;
  if (download_mgr) {
    verify_results = download_mgr->verifyChunks(
      verify_url,
      verify_requests,
      verify_headers,
      location_info->_disable_cert_check
    );
  }

  ////////////////////////////////////
  // Phase 3: Determine which chunks need upload
  ////////////////////////////////////

  std::vector<size_t> upload_indices;
  std::vector<size_t> already_valid_indices;

  if (verify_results.size() != available_chunk_indices.size()) {
    logchan_catalog->log("WARNING: Verification returned %zu results for %zu chunks, uploading all",
                         verify_results.size(), available_chunk_indices.size());
    upload_indices = available_chunk_indices; // Upload everything if verification failed
  } else {
    for (size_t i = 0; i < verify_results.size(); ++i) {
      size_t chunk_idx = available_chunk_indices[i];
      const auto& result = verify_results[i];

      if (result.present && result.hash_ok) {
        already_valid_indices.push_back(chunk_idx);
      } else {
        upload_indices.push_back(chunk_idx);
      }
    }

    logchan_catalog->log("  Server status: %zu already valid, %zu need upload",
                         already_valid_indices.size(), upload_indices.size());
  }

  if (upload_indices.empty()) {
    logchan_catalog->log("All chunks already present on server with valid hashes - nothing to upload");
    receipt->success = true;
    receipt->status_message = "All chunks already on server";
    receipt->bytes_uploaded = 0;
    return receipt;
  }

  ////////////////////////////////////
  // Phase 4: Build upload lists (only for chunks that need upload)
  ////////////////////////////////////

  std::vector<file::Path> chunk_files;
  std::vector<std::string> chunk_remote_paths;
  std::vector<URL> chunk_urls;
  std::vector<size_t> chunk_sizes;

  logchan_catalog->log("Uploading %zu chunks", upload_indices.size());

  for (size_t chunk_idx : upload_indices) {
    const auto& chunk = _chunk_manifest->_chunks[chunk_idx];

    std::string chunk_filename = catalog->getChunkFilename(_storage_hash, chunk_idx);
    file::Path chunk_path = catalog->getChunksDir() / chunk_filename;

    chunk_files.push_back(chunk_path);

    URL chunk_url = location_info->_upload_url / chunk_filename;
    chunk_urls.push_back(chunk_url);

    std::string url_path = chunk_url._path;
    size_t last_slash = url_path.rfind('/');
    std::string remote_path = (last_slash != std::string::npos)
                              ? url_path.substr(last_slash + 1)
                              : url_path;
    chunk_remote_paths.push_back(remote_path);
    chunk_sizes.push_back(chunk._size);
  }
  
  ////////////////////////////////////          
  // Create HTTPS upload-config from location info
  ////////////////////////////////////          

  auto https_config = std::make_shared<HttpsUploaderConfig>();
  
  ////////////////////////////////////          
  // Parse the first chunk URL to get host/port settings
  ////////////////////////////////////          

  if (!chunk_urls.empty()) {
    const URL& first_url = chunk_urls[0];
    https_config->host = first_url._host;
    https_config->port = first_url._port;
    https_config->verify_ssl = !location_info->_disable_cert_check;
    if (location_info->_api_key_write.has_value()) {
      https_config->api_key = location_info->_api_key_write.value();
    }
    
    // Extract base path from URL (everything before the filename)
    std::string url_path = first_url._path;
    size_t last_slash = url_path.rfind('/');
    https_config->remote_base_path = (last_slash != std::string::npos) //
                                   ? url_path.substr(0, last_slash)    //
                                   : "/";
  }
  
  ////////////////////////////////////          
  // HTTPS batch upload !
  ////////////////////////////////////          

  HttpsUploader uploader(https_config);
  bool chunks_success = uploader.uploadFiles(chunk_files, chunk_remote_paths);

  // Invoke chunk completion callback for each successfully uploaded chunk
  if (chunks_success && on_chunk_completed) {
    for (size_t i = 0; i < chunk_remote_paths.size(); ++i) {
      on_chunk_completed(chunk_remote_paths[i]);
    }
  }

  if (!chunks_success) {
    // Some or all chunks failed - add failure entries
    for (size_t i = 0; i < chunk_files.size(); ++i) {
      UploadFileEntry chunk_entry;
      chunk_entry.relative_path = chunk_files[i].getName();
      chunk_entry.remote_path = chunk_urls[i].toString();
      chunk_entry.size = chunk_sizes[i];
      chunk_entry.success = false;
      chunk_entry.error_message = "Batch upload failed";
      receipt->files.push_back(chunk_entry);
      receipt->failed_files++;
    }
    
    receipt->success = false;
    receipt->status_message = "Failed to upload chunks";
    saveReceipt(receipt);
    return receipt;
  }
  
  ////////////////////////////////////          
  // All chunks uploaded successfully
  //  mark so in receipt
  ////////////////////////////////////          

  if(0)logchan_catalog->log("Successfully uploaded all %zu chunks concurrently", _chunk_manifest->_chunks.size());
  
  for (size_t i = 0; i < chunk_files.size(); ++i) {
    UploadFileEntry chunk_entry;
    chunk_entry.relative_path = chunk_files[i].getName();
    chunk_entry.remote_path = chunk_urls[i].toString();
    chunk_entry.size = chunk_sizes[i];
    chunk_entry.hash = std::to_string(_chunk_manifest->_chunks[i]._hash);
    chunk_entry.success = true;
    receipt->files.push_back(chunk_entry);
    receipt->bytes_uploaded += chunk_sizes[i];
    receipt->successful_files++;
  }
  
  ////////////////////////////////////          
  // Set final receipt status
  ////////////////////////////////////          

  receipt->total_files = receipt->files.size();
  receipt->success = true;
  receipt->status_message = "Upload completed successfully";
  logchan_catalog->log("Upload receipt summary: %zu/%zu files successful", receipt->successful_files, receipt->total_files);
  
  ////////////////////////////////////          
  // Saving receipt to disk
  ////////////////////////////////////          

  saveReceipt(receipt);

  return receipt;
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::saveReceipt() - Save upload receipt
////////////////////////////////////////////////////////////////////////////////

void AssetEntry::saveReceipt(uploadreceipt_ptr_t receipt) const {
  auto manifest = getParentManifest();
  if (!manifest) return;
  
  auto catalog = manifest->getParentCatalog();
  if (!catalog) return;
  
  auto receipts_dir = catalog->getReceiptsDir() / _namespace;
  
  // Ensure directory exists
  boost::filesystem::create_directories(receipts_dir.toBFS());
  
  // Generate filename with storage hash
  auto filename = _id + "_" + _namespace + "_" + _storage_hash + ".json";
  auto receipt_path = receipts_dir / filename;
  
  // Save using built-in method
  receipt->saveToFile(receipt_path);
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::formatChunkIndex() - Format chunk index with leading zeros
////////////////////////////////////////////////////////////////////////////////

std::string AssetEntry::formatChunkIndex(int index) const {
  return FormatString("%04d", index);
}

} //namespace ork::asset::catalog {
