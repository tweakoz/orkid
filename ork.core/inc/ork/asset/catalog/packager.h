////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/kernel/datablock.h>
#include <ork/util/crypt.h>
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/chunk_manifest.h>
#include <ork/kernel/opq.h>
#include <functional>
#include <atomic>

namespace ork::asset::catalog {

// Type aliases moved to types.h

////////////////////////////////////////////////////////////////////////////////
// Configuration for asset packaging
////////////////////////////////////////////////////////////////////////////////

struct PackageConfig {
  // Compression settings
  CompressionType compression_type = CompressionType::LZ4;
  int compression_level = 0;  // 0 for LZ4 fast, 1-9 for LZ4HC
  
  // Encryption settings
  bool enable_encryption = false;
  std::string namespace_id;       // Used to lookup codec from catalog
  
  // Chunking settings
  size_t chunk_threshold = 10 * 1024 * 1024;  // Files larger than this get chunked (10MB default)
  size_t chunk_size = 4 * 1024 * 1024;        // Size of each chunk (4MB default)
  
  // Output settings
  file::Path output_dir;          // Where to write packaged assets
  bool preserve_directory_structure = true;
  bool generate_manifest = true;
  
  // Processing options
  opq::opq_ptr_t work_queue;      // OPQ for parallel processing
  bool verbose = false;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Validation
  ////////////////////////////////////////////////////////////////////////////////
  bool isValid() const;
  std::string getValidationError() const;
};

////////////////////////////////////////////////////////////////////////////////
// Result of packaging a single asset
////////////////////////////////////////////////////////////////////////////////

struct AssetPackageResult {
  std::string asset_path;         // Relative path of asset
  bool success = false;
  std::string error_message;
  
  // Output info
  size_t original_size = 0;
  size_t compressed_size = 0;
  float compression_ratio = 0.0f;
  std::string storage_hash;  // Storage hash (encrypted data) - for CAFS naming
  std::string content_hash;  // Content hash (raw data) - for verification
  
  // Chunk info (if applicable)
  chunkmanifest_ptr_t chunk_manifest;
  chunk_file_list_t chunk_files;
  
  // Timing
  double processing_time = 0.0;
};

////////////////////////////////////////////////////////////////////////////////
// Overall packaging result
////////////////////////////////////////////////////////////////////////////////

struct PackageResult {
  bool success = false;
  package_result_list_t asset_results;
  file::Path manifest_path;
  
  // Statistics
  size_t total_assets = 0;
  size_t successful_assets = 0;
  size_t failed_assets = 0;
  size_t total_original_size = 0;
  size_t total_compressed_size = 0;
  double total_time = 0.0;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Get summary string
  ////////////////////////////////////////////////////////////////////////////////
  std::string getSummary() const;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Get list of failed assets
  ////////////////////////////////////////////////////////////////////////////////
  failed_asset_list_t getFailedAssets() const;
};

////////////////////////////////////////////////////////////////////////////////
// Main asset packager class
// Handles compression, encryption, and chunking of assets
//
// Key features:
// - Codec reuse: Gets encryption codec from AssetCatalog hierarchy
// - Parallel processing using OPQ (operation queue)
// - Automatic chunking for large files (configurable threshold)
// - LZ4/LZ4HC compression with configurable levels
// - Progress tracking and cancellation support
// - Generates manifest entries with full metadata (size, hash, chunk info)
//
// Typical workflow:
// 1. Create PackageConfig with compression settings
// 2. Create AssetPackager via factory method (gets codec from catalog)
// 3. Package directory or individual files
// 4. Catalog's codec is used for all encryption (ensures consistency)
////////////////////////////////////////////////////////////////////////////////

struct AssetPackager {
  ////////////////////////////////////////////////////////////////////////////////
  // Factory method - validates config and creates packager
  // Gets encryption codec from catalog based on namespace_id in config
  ////////////////////////////////////////////////////////////////////////////////
  static assetpackager_ptr_t create(
    packageconfig_ptr_t config,
    assetcatalog_ptr_t catalog  // For codec lookup
  );
  
  ~AssetPackager();
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // Package a single file
  // Useful for testing or specific asset updates
  ////////////////////////////////////////////////////////////////////////////////
  AssetPackageResult packageFile(
    const file::Path& source_file,
    const file::Path& relative_path  // Path within the asset namespace
  );
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress callback
  // Called for each asset as it's processed
  // Uses pysafe type for Python GIL compatibility
  ////////////////////////////////////////////////////////////////////////////////
  // Progress callback type defined in type aliases above
  
  ////////////////////////////////////////////////////////////////////////////////
  // Cancel packaging operation
  ////////////////////////////////////////////////////////////////////////////////
  void cancel();
  bool isCancelled() const;
  
  // Constructor (private - use factory method)
  AssetPackager(
    packageconfig_ptr_t config,
    encryptioncodec_ptr_t codec  // From catalog
  );
  
  // Progress callback setter
  void setProgressCallback(pysafe_packager_progress_t callback);
  
private:
  // Implementation
  svar64_t _impl;
};

////////////////////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////////////////////

// Determine if a file should be chunked based on size and config
bool shouldChunkFile(const file::Path& path, const PackageConfig& config);

// Calculate compression ratio percentage
float calculateCompressionRatio(size_t original, size_t compressed);


} // namespace ork::asset::catalog