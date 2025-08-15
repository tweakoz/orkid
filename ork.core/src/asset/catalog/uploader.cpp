////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/uploader.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <boost/filesystem.hpp>

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// Implementation structures
////////////////////////////////////////////////////////////////

struct AssetUploaderAdapterImpl {
  AssetUploaderAdapter* _adapter;
  uploader_ptr_t _uploader;
  uploadconfig_ptr_t _config;
  pysafe_upload_progress_callback_t _progress_callback;
  
  // Current upload state
  UploadProgress _current_progress;
  Timer _upload_timer;
  
  AssetUploaderAdapterImpl(AssetUploaderAdapter* adapter, uploader_ptr_t uploader, uploadconfig_ptr_t config)
      : _adapter(adapter), _uploader(uploader), _config(config) {}
  
  // Internal methods
  void onUploaderProgress(size_t uploaded, size_t total);
  uploadreceipt_ptr_t createReceipt(assetmanifest_ptr_t manifest, const file_upload_result_list_t& results) const;
};

struct AssetUploadCoordinatorImpl {
  AssetUploadCoordinator* _coordinator;
  LockedResource<uploader_map_t> _uploaders;
  pysafe_aggregate_upload_progress_t _aggregate_progress_callback;
  
  AssetUploadCoordinatorImpl(AssetUploadCoordinator* coordinator)
      : _coordinator(coordinator) {}
};

////////////////////////////////////////////////////////////////
// AssetUploaderAdapter implementations moved from header
////////////////////////////////////////////////////////////////

void AssetUploaderAdapter::cancel() {
  auto impl = _impl.getShared<AssetUploaderAdapterImpl>();
  if (impl->_uploader) impl->_uploader->cancel();
}

bool AssetUploaderAdapter::isCancelled() const {
  auto impl = _impl.getShared<AssetUploaderAdapterImpl>();
  return impl->_uploader ? impl->_uploader->isCancelled() : false;
}

std::string AssetUploaderAdapter::type() const {
  auto impl = _impl.getShared<AssetUploaderAdapterImpl>();
  return impl->_uploader ? impl->_uploader->type() : "unknown";
}

void AssetUploaderAdapter::setProgressCallback(pysafe_upload_progress_callback_t callback) {
  auto impl = _impl.getShared<AssetUploaderAdapterImpl>();
  impl->_progress_callback = callback;
}


////////////////////////////////////////////////////////////////
// UploadProgress
////////////////////////////////////////////////////////////////

float UploadProgress::getProgressPercent() const {
  if (total_bytes == 0) return 0.0f;
  return (float)bytes_uploaded / (float)total_bytes * 100.0f;
}

double UploadProgress::getTransferRate() const {
  if (elapsed_time <= 0) return 0;
  return bytes_uploaded / elapsed_time;
}

std::string UploadProgress::getRateString() const {
  double _rate = getTransferRate();
  if (_rate < 1024) {
    return FormatString("%.0f B/s", _rate);
  } else if (_rate < 1024 * 1024) {
    return FormatString("%.1f KB/s", _rate / 1024.0);
  } else {
    return FormatString("%.1f MB/s", _rate / (1024.0 * 1024.0));
  }
}

////////////////////////////////////////////////////////////////
// UploadReceipt
////////////////////////////////////////////////////////////////

// isSuccess() method not in header

std::string UploadReceipt::getSummary() const {
  if (success) {
    return FormatString("Upload successful: %zu files, %.2f KB in %.2fs",
                       successful_files,
                       bytes_uploaded / 1024.0,
                       total_duration);
  } else {
    return FormatString("Upload failed: %s", status_message.c_str());
  }
}

std::string UploadReceipt::toJson() const {
  rapidjson::Document doc;
  doc.SetObject();
  auto& allocator = doc.GetAllocator();
  
  // Add basic fields
  doc.AddMember("upload_id", rapidjson::Value(upload_id.c_str(), allocator), allocator);
  doc.AddMember("namespace_id", rapidjson::Value(namespace_id.c_str(), allocator), allocator);
  doc.AddMember("manifest_id", rapidjson::Value(manifest_id.c_str(), allocator), allocator);
  doc.AddMember("destination", rapidjson::Value(destination.c_str(), allocator), allocator);
  doc.AddMember("timestamp", rapidjson::Value((int64_t)timestamp), allocator);
  doc.AddMember("total_files", rapidjson::Value((int64_t)total_files), allocator);
  doc.AddMember("successful_files", rapidjson::Value((int64_t)successful_files), allocator);
  doc.AddMember("failed_files", rapidjson::Value((int64_t)failed_files), allocator);
  doc.AddMember("bytes_uploaded", rapidjson::Value((int64_t)bytes_uploaded), allocator);
  doc.AddMember("total_duration", rapidjson::Value(total_duration), allocator);
  doc.AddMember("success", rapidjson::Value(success), allocator);
  doc.AddMember("status_message", rapidjson::Value(status_message.c_str(), allocator), allocator);
  
  // Add files array
  rapidjson::Value files_array(rapidjson::kArrayType);
  for (const auto& file : files) {
    rapidjson::Value file_obj(rapidjson::kObjectType);
    file_obj.AddMember("relative_path", rapidjson::Value(file.relative_path.c_str(), allocator), allocator);
    file_obj.AddMember("remote_path", rapidjson::Value(file.remote_path.c_str(), allocator), allocator);
    file_obj.AddMember("size", rapidjson::Value((int64_t)file.size), allocator);
    file_obj.AddMember("hash", rapidjson::Value(file.hash.c_str(), allocator), allocator);
    file_obj.AddMember("success", rapidjson::Value(file.success), allocator);
    file_obj.AddMember("error_message", rapidjson::Value(file.error_message.c_str(), allocator), allocator);
    files_array.PushBack(file_obj, allocator);
  }
  doc.AddMember("files", files_array, allocator);
  
  // Add warnings array
  rapidjson::Value warnings_array(rapidjson::kArrayType);
  for (const auto& warning : warnings) {
    warnings_array.PushBack(rapidjson::Value(warning.c_str(), allocator), allocator);
  }
  doc.AddMember("warnings", warnings_array, allocator);
  
  // Add errors array
  rapidjson::Value errors_array(rapidjson::kArrayType);
  for (const auto& error : errors) {
    errors_array.PushBack(rapidjson::Value(error.c_str(), allocator), allocator);
  }
  doc.AddMember("errors", errors_array, allocator);
  
  // Serialize to string
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);
  
  return buffer.GetString();
}

void UploadReceipt::fromJson(const std::string& json) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  
  if (doc.HasParseError() || !doc.IsObject()) {
    return;
  }
  
  // Extract basic fields
  if (doc.HasMember("upload_id") && doc["upload_id"].IsString())
    upload_id = doc["upload_id"].GetString();
  if (doc.HasMember("namespace_id") && doc["namespace_id"].IsString())
    namespace_id = doc["namespace_id"].GetString();
  if (doc.HasMember("manifest_id") && doc["manifest_id"].IsString())
    manifest_id = doc["manifest_id"].GetString();
  if (doc.HasMember("destination") && doc["destination"].IsString())
    destination = doc["destination"].GetString();
  if (doc.HasMember("timestamp") && doc["timestamp"].IsInt64())
    timestamp = doc["timestamp"].GetInt64();
  if (doc.HasMember("total_files") && doc["total_files"].IsInt64())
    total_files = doc["total_files"].GetInt64();
  if (doc.HasMember("successful_files") && doc["successful_files"].IsInt64())
    successful_files = doc["successful_files"].GetInt64();
  if (doc.HasMember("failed_files") && doc["failed_files"].IsInt64())
    failed_files = doc["failed_files"].GetInt64();
  if (doc.HasMember("bytes_uploaded") && doc["bytes_uploaded"].IsInt64())
    bytes_uploaded = doc["bytes_uploaded"].GetInt64();
  if (doc.HasMember("total_duration") && doc["total_duration"].IsDouble())
    total_duration = doc["total_duration"].GetDouble();
  if (doc.HasMember("success") && doc["success"].IsBool())
    success = doc["success"].GetBool();
  if (doc.HasMember("status_message") && doc["status_message"].IsString())
    status_message = doc["status_message"].GetString();
  
  // Extract files
  files.clear();
  if (doc.HasMember("files") && doc["files"].IsArray()) {
    const auto& files_array = doc["files"];
    for (rapidjson::SizeType i = 0; i < files_array.Size(); i++) {
      const auto& file_obj = files_array[i];
      if (file_obj.IsObject()) {
        UploadFileEntry entry;
        if (file_obj.HasMember("relative_path") && file_obj["relative_path"].IsString())
          entry.relative_path = file_obj["relative_path"].GetString();
        if (file_obj.HasMember("remote_path") && file_obj["remote_path"].IsString())
          entry.remote_path = file_obj["remote_path"].GetString();
        if (file_obj.HasMember("size") && file_obj["size"].IsInt64())
          entry.size = file_obj["size"].GetInt64();
        if (file_obj.HasMember("hash") && file_obj["hash"].IsString())
          entry.hash = file_obj["hash"].GetString();
        if (file_obj.HasMember("success") && file_obj["success"].IsBool())
          entry.success = file_obj["success"].GetBool();
        if (file_obj.HasMember("error_message") && file_obj["error_message"].IsString())
          entry.error_message = file_obj["error_message"].GetString();
        files.push_back(entry);
      }
    }
  }
  
  // Extract warnings
  warnings.clear();
  if (doc.HasMember("warnings") && doc["warnings"].IsArray()) {
    const auto& warnings_array = doc["warnings"];
    for (rapidjson::SizeType i = 0; i < warnings_array.Size(); i++) {
      if (warnings_array[i].IsString()) {
        warnings.push_back(warnings_array[i].GetString());
      }
    }
  }
  
  // Extract errors
  errors.clear();
  if (doc.HasMember("errors") && doc["errors"].IsArray()) {
    const auto& errors_array = doc["errors"];
    for (rapidjson::SizeType i = 0; i < errors_array.Size(); i++) {
      if (errors_array[i].IsString()) {
        errors.push_back(errors_array[i].GetString());
      }
    }
  }
}

bool UploadReceipt::saveToFile(const file::Path& path) const {
  try {
    std::string json_str = toJson();
    
    // Ensure directory exists
    auto parent_dir = path.toBFS().parent_path();
    file::Path parent_path;
    parent_path.fromBFS(parent_dir);
    parent_path.ensureDirectoryExists();
    
    // Write to file
    File outputfile(path, EFM_WRITE);
    outputfile.Write(json_str.c_str(), json_str.length());
    outputfile.Close();
    
    return true;
  } catch (...) {
    return false;
  }
}

uploadreceipt_ptr_t UploadReceipt::loadFromFile(const file::Path& path) {
  try {
    if (!path.doesPathExist()) {
      return nullptr;
    }
    
    // Read file
    File inputfile(path, EFM_READ);
    size_t length = 0;
    inputfile.GetLength(length);
    
    std::string json_str;
    json_str.resize(length);
    inputfile.Read((void*)json_str.data(), length);
    inputfile.Close();
    
    // Parse JSON
    auto receipt = std::make_shared<UploadReceipt>();
    receipt->fromJson(json_str);
    
    return receipt;
  } catch (...) {
    return nullptr;
  }
}

upload_file_list_t UploadReceipt::getFailedFiles() const {
  upload_file_list_t failed;
  for (const auto& file : files) {
    if (!file.success) {
      failed.push_back(file.relative_path);
    }
  }
  return failed;
}

////////////////////////////////////////////////////////////////
// AssetUploaderAdapter
////////////////////////////////////////////////////////////////

AssetUploaderAdapter::AssetUploaderAdapter(uploader_ptr_t uploader, uploadconfig_ptr_t config) {
  _impl.makeShared<AssetUploaderAdapterImpl>(this, uploader, config);
}

AssetUploaderAdapter::~AssetUploaderAdapter() {
}

uploadreceipt_ptr_t AssetUploaderAdapter::uploadManifest(
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir) {
  auto impl = _impl.getShared<AssetUploaderAdapterImpl>();
  auto receipt = std::make_shared<UploadReceipt>();
  
  // Initialize receipt
  receipt->namespace_id = manifest->getNamespace();
  receipt->manifest_id = manifest->getManifestId();
  receipt->destination = impl->_config->remote_base_path;
  receipt->timestamp = time(nullptr);
  
  // Start timer
  Timer upload_timer;
  upload_timer.Start();
  
  // Get all assets from manifest
  const auto& assets = manifest->getAssets();
  receipt->total_files = assets.size();
  
  // First, upload the manifest JSON file itself
  file::Path manifest_filename = FormatString("%s.manifest.json", manifest->getNamespace().c_str());
  file::Path manifest_path = source_dir / manifest_filename;
  
  if (manifest_path.doesPathExist()) {
    // Upload manifest file
    std::string manifest_remote_path = FormatString("manifests/%s", manifest_filename.c_str());
    if (!impl->_uploader->uploadFile(manifest_path, manifest_remote_path)) {
      receipt->warnings.push_back("Failed to upload manifest file");
    }
  }
  
  // Upload each asset
  size_t successful_uploads = 0;
  size_t failed_uploads = 0;
  size_t bytes_uploaded = 0;
  
  for (const auto& [_asset_id, entry] : assets) {
    // Skip assets that don't support current platform
    if (!entry->supportsCurrentPlatform()) {
      receipt->warnings.push_back(FormatString("Skipping %s - platform not supported", _asset_id.c_str()));
      continue;
    }
    
    UploadFileEntry file_entry;
    file_entry.relative_path = entry->_relative_path;
    
    try {
      if (uploadAssetFile(_asset_id, manifest, source_dir)) {
        successful_uploads++;
        bytes_uploaded += entry->_size;
        file_entry.success = true;
        file_entry.size = entry->_size;
        file_entry.hash = entry->_storage_hash;
        // Build remote URL from config and storage hash
        std::string remote_url = impl->_config->remote_base_path;
        if (!remote_url.empty() && remote_url.back() != '/') {
          remote_url += "/";
        }
        remote_url += entry->_storage_hash + ".enc";
        file_entry.remote_path = remote_url;
      } else {
        failed_uploads++;
        file_entry.success = false;
        file_entry.error_message = "Upload failed";
        receipt->errors.push_back(FormatString("Failed to upload %s", _asset_id.c_str()));
      }
    } catch (const std::exception& e) {
      failed_uploads++;
      file_entry.success = false;
      file_entry.error_message = e.what();
      receipt->errors.push_back(FormatString("Exception uploading %s: %s", _asset_id.c_str(), e.what()));
    }
    
    receipt->files.push_back(file_entry);
    
    // Call progress callback
    if (impl->_progress_callback._item) {
      UploadProgress progress;
      progress.current_file = _asset_id;
      progress.files_completed = successful_uploads;
      progress.total_files = receipt->total_files;
      progress.bytes_uploaded = bytes_uploaded;
      progress.total_bytes = manifest->getTotalSize();
      progress.elapsed_time = upload_timer.SecsSinceStart();
      
      impl->_progress_callback._item(progress);
    }
    
    // Check if cancelled
    if (isCancelled()) {
      receipt->status_message = "Upload cancelled";
      receipt->success = false;
      receipt->successful_files = successful_uploads;
      receipt->failed_files = failed_uploads + (receipt->total_files - successful_uploads - failed_uploads);
      receipt->bytes_uploaded = bytes_uploaded;
      receipt->total_duration = upload_timer.SecsSinceStart();
      return receipt;
    }
  }
  
  // Finalize receipt
  receipt->successful_files = successful_uploads;
  receipt->failed_files = failed_uploads;
  receipt->bytes_uploaded = bytes_uploaded;
  receipt->total_duration = upload_timer.SecsSinceStart();
  receipt->success = (failed_uploads == 0);
  
  if (receipt->success) {
    receipt->status_message = FormatString("Successfully uploaded %zu files", successful_uploads);
  } else {
    receipt->status_message = FormatString("Upload completed with %zu failures", failed_uploads);
  }
  
  return receipt;
}

uploadreceipt_ptr_t AssetUploaderAdapter::uploadAssetFiles(
    const upload_file_list_t& _asset_ids,
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir) {
  auto impl = _impl.getShared<AssetUploaderAdapterImpl>();
  auto receipt = std::make_shared<UploadReceipt>();
  
  // Initialize receipt
  receipt->namespace_id = manifest->getNamespace();
  receipt->manifest_id = manifest->getManifestId();
  receipt->destination = impl->_config->remote_base_path;
  receipt->timestamp = time(nullptr);
  receipt->total_files = _asset_ids.size();
  
  // Start timer
  Timer upload_timer;
  upload_timer.Start();
  
  // Get all assets from manifest
  const auto& assets = manifest->getAssets();
  
  // Upload each requested asset
  size_t successful_uploads = 0;
  size_t failed_uploads = 0;
  size_t bytes_uploaded = 0;
  
  for (const auto& _asset_id : _asset_ids) {
    // Find asset in manifest
    auto it = assets.find(_asset_id);
    if (it == assets.end()) {
      receipt->errors.push_back(FormatString("Asset not found in manifest: %s", _asset_id.c_str()));
      failed_uploads++;
      continue;
    }
    
    auto entry = it->second;
    
    // Skip assets that don't support current platform
    if (!entry->supportsCurrentPlatform()) {
      receipt->warnings.push_back(FormatString("Skipping %s - platform not supported", _asset_id.c_str()));
      continue;
    }
    
    UploadFileEntry file_entry;
    file_entry.relative_path = entry->_relative_path;
    
    try {
      if (uploadAssetFile(_asset_id, manifest, source_dir)) {
        successful_uploads++;
        bytes_uploaded += entry->_size;
        file_entry.success = true;
        file_entry.size = entry->_size;
        file_entry.hash = entry->_storage_hash;
        // Build remote URL from config and storage hash
        std::string remote_url = impl->_config->remote_base_path;
        if (!remote_url.empty() && remote_url.back() != '/') {
          remote_url += "/";
        }
        remote_url += entry->_storage_hash + ".enc";
        file_entry.remote_path = remote_url;
      } else {
        failed_uploads++;
        file_entry.success = false;
        file_entry.error_message = "Upload failed";
        receipt->errors.push_back(FormatString("Failed to upload %s", _asset_id.c_str()));
      }
    } catch (const std::exception& e) {
      failed_uploads++;
      file_entry.success = false;
      file_entry.error_message = e.what();
      receipt->errors.push_back(FormatString("Exception uploading %s: %s", _asset_id.c_str(), e.what()));
    }
    
    receipt->files.push_back(file_entry);
    
    // Call progress callback
    if (impl->_progress_callback._item) {
      UploadProgress progress;
      progress.current_file = _asset_id;
      progress.files_completed = successful_uploads;
      progress.total_files = receipt->total_files;
      progress.bytes_uploaded = bytes_uploaded;
      // Calculate total bytes for just requested files
      size_t total_requested_bytes = 0;
      for (const auto& id : _asset_ids) {
        auto asset_it = assets.find(id);
        if (asset_it != assets.end()) {
          total_requested_bytes += asset_it->second->_size;
        }
      }
      progress.total_bytes = total_requested_bytes;
      progress.elapsed_time = upload_timer.SecsSinceStart();
      
      impl->_progress_callback._item(progress);
    }
    
    // Check if cancelled
    if (isCancelled()) {
      receipt->status_message = "Upload cancelled";
      receipt->success = false;
      receipt->successful_files = successful_uploads;
      receipt->failed_files = failed_uploads + (receipt->total_files - successful_uploads - failed_uploads);
      receipt->bytes_uploaded = bytes_uploaded;
      receipt->total_duration = upload_timer.SecsSinceStart();
      return receipt;
    }
  }
  
  // Finalize receipt
  receipt->successful_files = successful_uploads;
  receipt->failed_files = failed_uploads;
  receipt->bytes_uploaded = bytes_uploaded;
  receipt->total_duration = upload_timer.SecsSinceStart();
  receipt->success = (failed_uploads == 0);
  
  if (receipt->success) {
    receipt->status_message = FormatString("Successfully uploaded %zu files", successful_uploads);
  } else {
    receipt->status_message = FormatString("Upload completed with %zu failures", failed_uploads);
  }
  
  return receipt;
}

bool AssetUploaderAdapter::uploadAssetFile(
    const std::string& _asset_id,
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir) {
  auto impl = _impl.getShared<AssetUploaderAdapterImpl>();
  
  
  if (!impl->_uploader) {
    logchan_catalog->log("ERROR: No uploader available");
    return false;
  }
  
  // Find the asset entry in the manifest
  const auto& assets = manifest->getAssets();
  
  auto it = assets.find(_asset_id);
  if (it == assets.end()) {
    logchan_catalog->log("ERROR: Asset '%s' not found in manifest", _asset_id.c_str());
    return false; // Asset not found in manifest
  }
  auto entry = it->second;
  
  // Construct source file path - use storage_hash.enc as the filename
  auto source_file = source_dir / (entry->_storage_hash + ".enc");
  
  if (!source_file.doesPathExist()) {
    logchan_catalog->log("ERROR: Source file doesn't exist: '%s'", source_file.c_str());
    return false; // Source file doesn't exist
  }
  
  // Build full remote URL for content addressable filesystem
  if (entry->_remote_loc.empty() || entry->_storage_hash.empty()) {
    logchan_catalog->log("ERROR: Failed to build remote URL (missing remote_loc or hash)");
    return false;
  }
  
  std::string remote_url = impl->_config->remote_base_path;
  if (!remote_url.empty() && remote_url.back() != '/') {
    remote_url += "/";
  }
  remote_url += entry->_storage_hash + ".enc";
  
  // Check if this is a chunked asset
  if (entry->isChunked()) {
    // Upload chunked asset
    auto chunk_manifest = entry->_chunk_manifest;
    if (!chunk_manifest) {
      logchan_catalog->log("ERROR: Asset marked as chunked but no chunk manifest found");
      return false;
    }
    
    // Upload each chunk with index-based naming convention
    // Format: {storage_hash}.enc.chunk.{index:04d}
    bool all_chunks_uploaded = true;
    
    for (size_t chunk_idx = 0; chunk_idx < chunk_manifest->_chunks.size(); ++chunk_idx) {
      // Construct chunk filename
      std::string chunk_filename = FormatString("%s.enc.chunk.%04zu", 
                                               entry->_storage_hash.c_str(), 
                                               chunk_idx);
      
      // Construct source path for chunk
      file::Path chunk_source = source_dir / chunk_filename;
      
      if (!chunk_source.doesPathExist()) {
        logchan_catalog->log("ERROR: Chunk file doesn't exist: '%s'", chunk_source.c_str());
        all_chunks_uploaded = false;
        break;
      }
      
      // Upload the chunk
      try {
        if (!impl->_uploader->uploadFile(chunk_source, chunk_filename)) {
          logchan_catalog->log("ERROR: Failed to upload chunk %zu of asset '%s'", chunk_idx, _asset_id.c_str());
          all_chunks_uploaded = false;
          break;
        }
      } catch (const std::exception& e) {
        logchan_catalog->log("ERROR: Exception uploading chunk %zu: %s", chunk_idx, e.what());
        all_chunks_uploaded = false;
        break;
      }
    }
    
    return all_chunks_uploaded;
  }
  
  // For now, extract just the filename for the uploader
  // TODO: Update uploaders to handle full URLs properly
  auto remote_path = entry->_storage_hash + ".enc";
  
  try {
    // Perform the upload using the underlying uploader
    bool result = impl->_uploader->uploadFile(source_file, remote_path);
    return result;
  } catch (const std::exception& e) {
    logchan_catalog->log("ERROR: Upload exception: %s", e.what());
    return false;
  } catch (...) {
    logchan_catalog->log("ERROR: Unknown upload exception");
    return false;
  }
}

// Implementation methods moved to AssetUploaderAdapterImpl

void AssetUploaderAdapterImpl::onUploaderProgress(size_t uploaded, size_t total) {
  // TODO: Convert generic progress to asset progress
}

uploadreceipt_ptr_t AssetUploaderAdapterImpl::createReceipt(
    assetmanifest_ptr_t manifest,
    const file_upload_result_list_t& results) const {
  auto receipt = std::make_shared<UploadReceipt>();
  
  // TODO: Create receipt from results
  receipt->success = false;
  receipt->status_message = "Not implemented";
  
  return receipt;
}

////////////////////////////////////////////////////////////////
// AssetUploadCoordinator
////////////////////////////////////////////////////////////////

AssetUploadCoordinator::AssetUploadCoordinator() {
  _impl.makeShared<AssetUploadCoordinatorImpl>(this);
}

AssetUploadCoordinator::~AssetUploadCoordinator() {
  // TODO: Cancel all uploads
}

void AssetUploadCoordinator::registerUploader(const std::string& name, assetuploaderadapter_ptr_t uploader) {
  auto impl = _impl.getShared<AssetUploadCoordinatorImpl>();
  impl->_uploaders.atomicOp([&](uploader_map_t& uploaders) {
    uploaders[name] = uploader;
  });
}

assetuploaderadapter_ptr_t AssetUploadCoordinator::getUploader(const std::string& name) const {
  auto impl = _impl.getShared<AssetUploadCoordinatorImpl>();
  assetuploaderadapter_ptr_t result = nullptr;
  impl->_uploaders.atomicOp([&](const uploader_map_t& uploaders) {
    auto it = uploaders.find(name);
    if (it != uploaders.end()) {
      result = it->second;
    }
  });
  return result;
}

uploader_name_list_t AssetUploadCoordinator::listUploaders() const {
  auto impl = _impl.getShared<AssetUploadCoordinatorImpl>();
  uploader_name_list_t names;
  impl->_uploaders.atomicOp([&](const uploader_map_t& uploaders) {
    for (const auto& [name, uploader] : uploaders) {
      names.push_back(name);
    }
  });
  return names;
}

void AssetUploadCoordinator::setAggregateProgressCallback(pysafe_aggregate_upload_progress_t callback) {
  auto impl = _impl.getShared<AssetUploadCoordinatorImpl>();
  impl->_aggregate_progress_callback = callback;
}

upload_result_map_t AssetUploadCoordinator::uploadToMultiple(
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir,
    const uploader_name_list_t& uploader_names) {
  upload_result_map_t results;
  
  for (const auto& name : uploader_names) {
    auto uploader = getUploader(name);
    if (uploader) {
      auto receipt = uploader->uploadManifest(manifest, source_dir);
      results[name] = receipt;
    }
  }
  
  return results;
}

uploadreceipt_ptr_t AssetUploadCoordinator::uploadWithFallback(
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir,
    const uploader_name_list_t& uploader_names) {
  for (const auto& name : uploader_names) {
    auto uploader = getUploader(name);
    if (uploader) {
      auto receipt = uploader->uploadManifest(manifest, source_dir);
      if (receipt && receipt->success) {
        return receipt;
      }
    }
  }
  
  return nullptr;
}

// AssetUploadCoordinator methods that are not in header - removing

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

assetuploaderadapter_ptr_t createAssetUploader(
    const std::string& type, 
    uploadconfig_ptr_t config) {
  // This factory function now expects protocol-specific configs to be passed in
  // Cast the base config to the appropriate protocol config
  uploader_ptr_t base_uploader = nullptr;
  
  if (type == "https" || type == "http") {
    // Config should already be HttpsUploaderConfig
    auto https_config = std::dynamic_pointer_cast<HttpsUploaderConfig>(config);
    if (!https_config) {
      return nullptr; // Wrong config type
    }
    base_uploader = std::make_shared<HttpsUploader>(https_config);
  }
  else if (type == "scp" || type == "ssh") {
    // Config should already be ScpUploaderConfig
    auto scp_config = std::dynamic_pointer_cast<ScpUploaderConfig>(config);
    if (!scp_config) {
      return nullptr; // Wrong config type
    }
    base_uploader = std::make_shared<ScpUploader>(scp_config);
  }
  else if (type == "s3") {
    // Config should already be S3UploaderConfig
    auto s3_config = std::dynamic_pointer_cast<S3UploaderConfig>(config);
    if (!s3_config) {
      return nullptr; // Wrong config type
    }
    base_uploader = std::make_shared<S3Uploader>(s3_config);
  }
  
  if (base_uploader) {
    return std::make_shared<AssetUploaderAdapter>(base_uploader, config);
  }
  
  return nullptr;
}

assetuploaderadapter_ptr_t createAssetUploaderFromUrl(
    const URL& url, 
    uploadconfig_ptr_t config) {
  // URL factory creation logged at higher level if needed
  
  // Auto-detect type from URL scheme
  std::string type;
  
  if (url._scheme == "https") {
    type = "https";
  } else if (url._scheme == "http") {
    type = "http";
  } else if (url._scheme == "scp" || url._scheme == "ssh") {
    type = "scp";
  } else if (url._scheme == "s3") {
    type = "s3";
  } else {
    logchan_catalog->log("ERROR: Unsupported URL scheme: %s", url._scheme.c_str());
    return nullptr;
  }
  
  // Uploader type detection logged at higher level if needed
  
  // Create config if not provided
  if (!config) {
    // Config parsing logged at higher level if needed
    config = parseUploadUrl(url);
  } else {
    // Config usage logged at higher level if needed
  }
  
  auto result = createAssetUploader(type, config);
  // Uploader creation result logged at higher level if needed
  return result;
}

uploadconfig_ptr_t parseUploadUrl(const URL& url) {
  // Create protocol-specific config based on URL scheme
  uploadconfig_ptr_t config = nullptr;
  
  if (url._scheme == "https" || url._scheme == "http") {
    auto https_config = std::make_shared<HttpsUploaderConfig>();
    https_config->host = url._host;
    https_config->port = url._port > 0 ? url._port : (url._scheme == "https" ? 443 : 80);
    https_config->verify_ssl = (url._scheme == "https");
    
    // Parse username from userinfo if present
    if (!url._userinfo.empty()) {
      auto colon_pos = url._userinfo.find(':');
      if (colon_pos != std::string::npos) {
        https_config->username = url._userinfo.substr(0, colon_pos);
        https_config->password = url._userinfo.substr(colon_pos + 1);
      } else {
        https_config->username = url._userinfo;
      }
    }
    
    https_config->remote_base_path = url._path;
    https_config->timeout_seconds = 300;
    config = https_config;
  }
  else if (url._scheme == "scp" || url._scheme == "ssh") {
    auto scp_config = std::make_shared<ScpUploaderConfig>();
    scp_config->host = url._host;
    scp_config->port = url._port > 0 ? url._port : 22;
    
    // Parse username from userinfo if present
    if (!url._userinfo.empty()) {
      auto colon_pos = url._userinfo.find(':');
      if (colon_pos != std::string::npos) {
        scp_config->username = url._userinfo.substr(0, colon_pos);
        scp_config->password = url._userinfo.substr(colon_pos + 1);
      } else {
        scp_config->username = url._userinfo;
      }
    }
    
    scp_config->remote_base_path = url._path;
    scp_config->timeout_seconds = 300;
    config = scp_config;
  }
  else if (url._scheme == "s3") {
    auto s3_config = std::make_shared<S3UploaderConfig>();
    s3_config->bucket = url._host;  // S3 uses host as bucket name
    s3_config->remote_base_path = url._path;
    s3_config->timeout_seconds = 300;
    config = s3_config;
  }
  else {
    // Fallback to base config for unknown schemes
    config = std::make_shared<UploadConfig>();
    config->remote_base_path = url._path;
    config->timeout_seconds = 300;
  }
  
  return config;
}

} // namespace ork::asset::catalog