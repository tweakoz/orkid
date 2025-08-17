////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/file/path.h>
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/kernel/timer.h>
#include <ork/util/URL.h>
#include <ork/util/upload.h>
#include <ork/util/upload_manager.h>
#include <memory>
#include <functional>
#include <vector>
#include <map>

namespace ork::asset::catalog {

// Type aliases moved to types.h

////////////////////////////////////////////////////////////////////////////////
// Callback types are defined in types.h for consistency
// This includes both regular and Python-safe (ItemAndData) versions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Upload progress information
////////////////////////////////////////////////////////////////////////////////

struct UploadProgress {
  std::string current_file;
  size_t files_completed = 0;
  size_t total_files = 0;
  size_t bytes_uploaded = 0;
  size_t total_bytes = 0;
  double elapsed_time = 0.0;
  double estimated_time_remaining = 0.0;
  
  float getProgressPercent() const;
  double getTransferRate() const;
  std::string getRateString() const;
};

////////////////////////////////////////////////////////////////////////////////
// File entry for upload receipt
////////////////////////////////////////////////////////////////////////////////

struct UploadFileEntry {
  std::string relative_path;
  std::string remote_path;
  size_t size = 0;
  std::string hash;
  bool success = false;
  std::string error_message;
};

////////////////////////////////////////////////////////////////////////////////
// Upload receipt - proof of successful upload
////////////////////////////////////////////////////////////////////////////////

struct UploadReceipt {
  std::string upload_id;
  std::string namespace_id;
  std::string manifest_id;
  std::string destination;
  time_t timestamp = 0;
  
  upload_file_entry_list_t files;
  
  // Summary statistics
  size_t total_files = 0;
  size_t successful_files = 0;
  size_t failed_files = 0;
  size_t bytes_uploaded = 0;
  double total_duration = 0.0;
  
  // Overall status
  bool success = false;
  std::string status_message;
  upload_warning_list_t warnings;
  upload_error_list_t errors;
  
  // Chunk manifest for chunked uploads
  chunkmanifest_ptr_t _chunk_manifest;  // nullptr for non-chunked assets
  
  ////////////////////////////////////////////////////////////////////////////////
  // Serialization
  ////////////////////////////////////////////////////////////////////////////////
  std::string toJson() const;
  void fromJson(const std::string& json);
  
  // Save/load receipt
  bool saveToFile(const file::Path& path) const;
  static uploadreceipt_ptr_t loadFromFile(const file::Path& path);
  
  // Get summary string
  std::string getSummary() const;
  
  upload_file_list_t getFailedFiles() const;
};

////////////////////////////////////////////////////////////////////////////////
// Asset uploader adapters - wrap generic uploaders with asset-specific logic
//
// These adapters bridge between the generic upload infrastructure in util/
// and the asset-specific requirements of the catalog system:
// - Manifest-aware uploads
// - Upload receipt generation with asset metadata
// - Integration with asset namespace configuration
// - Progress tracking with asset context
//
// Typical workflow:
// 1. Package assets using AssetPackager
// 2. Create asset uploader adapter with appropriate uploader
// 3. Upload manifest or individual asset files
// 4. Receive upload receipt for verification
////////////////////////////////////////////////////////////////////////////////

struct AssetUploaderAdapter {
  AssetUploaderAdapter(uploader_ptr_t uploader, uploadconfig_ptr_t config);
  ~AssetUploaderAdapter();
  
  ////////////////////////////////////////////////////////////////////////////////
  // Main upload methods
  ////////////////////////////////////////////////////////////////////////////////
  
  // Upload a complete manifest (all files)
  uploadreceipt_ptr_t uploadManifest(
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir
  );
  
  // Upload specific asset files
  uploadreceipt_ptr_t uploadAssetFiles(
    const upload_file_list_t& asset_ids,
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir
  );
  
  // Upload a single asset file
  bool uploadAssetFile(
    const std::string& asset_id,
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress tracking
  // Uses pysafe type for Python GIL compatibility
  ////////////////////////////////////////////////////////////////////////////////
  
  
  void cancel();
  bool isCancelled() const;
  std::string type() const;
  
  // Progress callback setter
  void setProgressCallback(pysafe_upload_progress_callback_t callback);
  
private:
  // Implementation
  svar64_t _impl;
};

////////////////////////////////////////////////////////////////////////////////
// Asset upload coordinator - manages uploaders for asset catalog
////////////////////////////////////////////////////////////////////////////////

struct AssetUploadCoordinator {
  AssetUploadCoordinator();
  ~AssetUploadCoordinator();
  
  ////////////////////////////////////////////////////////////////////////////////
  // Uploader registration
  ////////////////////////////////////////////////////////////////////////////////
  
  // Register an uploader adapter
  void registerUploader(const std::string& name, assetuploaderadapter_ptr_t uploader);
  
  // Get uploader by name
  assetuploaderadapter_ptr_t getUploader(const std::string& name) const;
  
  // List registered uploaders
  uploader_name_list_t listUploaders() const;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Batch upload operations
  ////////////////////////////////////////////////////////////////////////////////
  
  // Upload to multiple destinations
  upload_result_map_t uploadToMultiple(
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir,
    const uploader_name_list_t& uploader_names
  );
  
  // Upload with fallback (try uploaders in order until one succeeds)
  uploadreceipt_ptr_t uploadWithFallback(
    assetmanifest_ptr_t manifest,
    const file::Path& source_dir,
    const uploader_name_list_t& uploader_names
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress aggregation
  // Uses pysafe type for Python GIL compatibility
  ////////////////////////////////////////////////////////////////////////////////
  
  void setAggregateProgressCallback(pysafe_aggregate_upload_progress_t callback);
  
private:
  // Implementation
  svar64_t _impl;
};

////////////////////////////////////////////////////////////////////////////////
// Factory functions
////////////////////////////////////////////////////////////////////////////////

// Create asset uploader adapter from config
assetuploaderadapter_ptr_t createAssetUploader(
  const std::string& type, 
  uploadconfig_ptr_t config
);

// Create asset uploader adapter from URL (auto-detect type)
assetuploaderadapter_ptr_t createAssetUploaderFromUrl(
  const URL& url, 
  uploadconfig_ptr_t config = nullptr
);

// Parse upload URL into config
uploadconfig_ptr_t parseUploadUrl(const URL& url);

// Type aliases moved to types.h

} // namespace ork::asset::catalog