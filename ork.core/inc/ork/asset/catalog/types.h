////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/crc.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/datablock.h>
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <regex>
#include <atomic>

namespace ork::file {
  class Path;
}

namespace ork {
  struct UploadConfig;
  struct HttpsUploaderConfig;
  
  // Pointer type aliases
  using uploadconfig_ptr_t = std::shared_ptr<UploadConfig>;
  using httpsuploaderconfig_ptr_t = std::shared_ptr<HttpsUploaderConfig>;
}

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////////////////////
// Forward declarations for all asset catalog types
////////////////////////////////////////////////////////////////////////////////

// Core types
struct AssetManifest;              // Registry of all assets in a namespace with metadata and locations
struct AssetEntry;                 // Single asset's metadata including size, hash, dependencies
struct AssetConfig;                // Configuration for asset catalog system (paths, cache settings, etc)
struct NamespaceConfig;            // Configuration specific to a namespace (encryption key, upload location)
struct AssetNamespace;             // Logical grouping of assets with manifest, codec, and metadata
struct AssetCatalog;               // Main interface for asset discovery, fetching, and caching
struct AssetLocation;              // Where to find an asset (URL, path, CDN endpoint)
struct DownloadProgress;           // Progress tracking for asset downloads
struct AssetConfigSpace;           // Container for multiple configurations

// Manifest and packaging
// ManifestEntry merged into AssetEntry
//struct AssetPackager;              // Compresses and encrypts assets for distribution
struct PackageConfig;              // Configuration for asset packaging (compression, chunking)
struct PackageResult;              // Overall result of packaging operation with statistics
struct AssetPackageResult;         // Result of packaging a single asset

// Chunking
struct ChunkManifest;              // Describes how a large file is split into chunks
struct ChunkMeta;                  // Metadata for a single chunk (size, hash, offset)
struct ChunkAssembler;             // Reassembles chunks back into original file
struct ChunkAssemblyResult;        // Result of chunk assembly operation
struct ChunkDownloadCoordinator;   // Manages parallel download of file chunks
struct ChunkUploadCoordinator;     // Manages parallel upload of file chunks

// Fetching
struct AssetFetcher;               // Downloads assets from remote locations with retry logic
struct LocationInfo;               // Information about an asset location (CDN, local, etc)

// Uploading
struct UploadReceipt;              // Proof of successful upload with file list and statistics
struct UploadFileEntry;            // Information about a single uploaded file
struct UploadProgress;             // Progress tracking for uploads
struct AssetUploaderAdapter;       // Wraps generic uploaders with asset-specific logic
struct AssetUploadCoordinator;     // Manages multiple uploaders for redundancy/fallback

// Async fetching
struct FetchRequest;               // Encapsulates all parameters for asset fetching
struct LocalManifest;
struct AssetFqIdentifier;
struct AssetIndexEntry;

////////////////////////////////////////////////////////////////////////////////
// Shared pointer aliases
////////////////////////////////////////////////////////////////////////////////

using assetmanifest_ptr_t = std::shared_ptr<AssetManifest>;
using assetmanifest_wkptr_t = std::weak_ptr<AssetManifest>;
using manifest_list_t = std::vector<assetmanifest_ptr_t>;
using assetentry_ptr_t = std::shared_ptr<AssetEntry>;
using assetconfig_ptr_t = std::shared_ptr<AssetConfig>;
using assetnamespace_ptr_t = std::shared_ptr<AssetNamespace>;
using assetcatalog_ptr_t = std::shared_ptr<AssetCatalog>;
using assetcatalog_wkptr_t = std::weak_ptr<AssetCatalog>;
using assetlocation_ptr_t = std::shared_ptr<AssetLocation>;
using assetconfigspace_ptr_t = std::shared_ptr<AssetConfigSpace>;
using fetchrequest_ptr_t = std::shared_ptr<FetchRequest>;
using assetfqid_ptr_t = std::shared_ptr<AssetFqIdentifier>;

// manifestentry_ptr_t removed - use assetentry_ptr_t instead
//using assetpackager_ptr_t = std::shared_ptr<AssetPackager>;
using packageconfig_ptr_t = std::shared_ptr<PackageConfig>;
using packageresult_ptr_t = std::shared_ptr<PackageResult>;

using chunkmanifest_ptr_t = std::shared_ptr<ChunkManifest>;
using chunkassembler_ptr_t = std::shared_ptr<ChunkAssembler>;
using chunkassemblyresult_ptr_t = std::shared_ptr<ChunkAssemblyResult>;
using chunkdownloadcoordinator_ptr_t = std::shared_ptr<ChunkDownloadCoordinator>;
using chunkuploadcoordinator_ptr_t = std::shared_ptr<ChunkUploadCoordinator>;

using assetfetcher_ptr_t = std::shared_ptr<AssetFetcher>;
using locationinfo_ptr_t = std::shared_ptr<LocationInfo>;

using uploadreceipt_ptr_t = std::shared_ptr<UploadReceipt>;
using assetuploaderadapter_ptr_t = std::shared_ptr<AssetUploaderAdapter>;
using assetuploadcoordinator_ptr_t = std::shared_ptr<AssetUploadCoordinator>;

using configlist_t = std::vector<assetconfig_ptr_t>;

using localmanifest_ptr_t = std::shared_ptr<LocalManifest>;

using assetindexentry_ptr_t = std::shared_ptr<AssetIndexEntry>;

////////////////////////////////////////////////////////////////////////////////
// Weak pointer aliases (only for types that actually use weak_ptr)
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// String type aliases for semantic clarity
////////////////////////////////////////////////////////////////////////////////

using assetid_t = std::string;
using assetid_list_t = std::vector<assetid_t>;
using namespaceid_t = std::string;

using assethandle_map_t = std::map<assetid_t, fetchrequest_ptr_t>;

////////////////////////////////////////////////////////////////////////////////
// Container type aliases
////////////////////////////////////////////////////////////////////////////////

using asset_diskpath_t = file::Path;
using path_list_t = std::vector<asset_diskpath_t>;

// Catalog type aliases
using downloadprogress_ptr_t = std::shared_ptr<DownloadProgress>;
using namespace_visitor_fn_t = std::function<void(const namespaceid_t& path, int depth)>;
using asset_predicate_fn_t = std::function<bool(const assetid_t& id, const assetentry_ptr_t& entry)>;
using task_fn_t = std::function<void()>;
using chunk_progress_fn_t = std::function<void(int chunk_index, float progress)>;

// Chunk type aliases
using chunk_index_t = size_t;
using chunk_size_t = size_t;
using chunk_offset_t = size_t;
using chunk_hash_t = uint64_t;
using chunk_data_map_t = std::map<chunk_index_t, datablock_ptr_t>;
using chunk_file_list_t = std::vector<file::Path>;
using chunk_meta_list_t = std::vector<ChunkMeta>;

// Config type aliases
using namespace_key_map_t = std::map<std::string, std::string>;
using namespaceconfig_ptr_t = std::shared_ptr<NamespaceConfig>;
using namespaceconfig_map_t = std::map<std::string, namespaceconfig_ptr_t>;
using remote_location_map_t = std::map<std::string, locationinfo_ptr_t>;
using local_location_map_t = std::map<std::string, file::Path>;

// Fetcher type aliases
using asset_entry_map_t = std::map<std::string, assetentry_ptr_t>;
using manifest_dir_list_t = std::vector<file::Path>;
using fetch_completion_fn_t = std::function<void()>;

// Manifest builder type aliases
using platform_list_t = std::vector<std::string>;
using dependency_map_t = std::map<std::string, std::string>;
using pattern_list_t = std::vector<std::string>;
using regex_list_t = std::vector<std::regex>;
using manifest_entry_list_t = std::vector<assetentry_ptr_t>;
using hash_cache_map_t = std::map<std::string, uint64_t>;
using progress_callback_fn_t = std::function<void(const std::string& file, size_t current, size_t total)>;
using filter_callback_fn_t = std::function<bool(const file::Path& path)>;

// Manifest type aliases
using asset_metadata_map_t = std::map<std::string, std::string>;
using asset_dependency_map_t = std::map<std::string, std::string>;
using validation_error_list_t = std::vector<std::string>;
using asset_type_count_map_t = std::map<std::string, size_t>;

// Namespace type aliases
using namespace_metadata_map_t = std::map<std::string, std::string>;
using namespace_component_list_t = std::vector<std::string>;
using namespace_list_t = std::vector<assetnamespace_ptr_t>;
using namespace_node_visitor_fn_t = std::function<void(const AssetNamespace&, int depth)>;

// Packager type aliases
using package_result_list_t = std::vector<AssetPackageResult>;
using failed_asset_list_t = std::vector<std::string>;
using packager_progress_fn_t = std::function<void(const std::string& asset_path, float progress)>;

// Uploader type aliases
using upload_file_list_t = std::vector<std::string>;
using upload_result_map_t = std::map<std::string, uploadreceipt_ptr_t>;
using uploader_map_t = std::map<std::string, assetuploaderadapter_ptr_t>;
using file_upload_result_list_t = std::vector<std::pair<std::string, bool>>;
using upload_file_entry_list_t = std::vector<UploadFileEntry>;
using uploader_name_list_t = std::vector<std::string>;
using upload_error_list_t = std::vector<std::string>;
using upload_warning_list_t = std::vector<std::string>;
using upload_aggregate_progress_fn_t = std::function<void(const std::string&, const UploadProgress&)>;

////////////////////////////////////////////////////////////////////////////////
// Enums - Using CrcEnum for Python compatibility (converts to crcstring)
////////////////////////////////////////////////////////////////////////////////

// Compression types supported by the asset system
enum class CompressionType : ::ork::crc_enum_t {
  CrcEnum(NONE),   // No compression
  CrcEnum(LZ4),    // LZ4 fast compression
  CrcEnum(LZ4HC)   // LZ4 high compression
};

////////////////////////////////////////////////////////////////////////////////
// Error handling policy
// - Fatal errors (corruption, programming errors) → OrkAssert(false)
// - Network/IO errors → Return error code/status
// - Optional asset failures → Return error, caller decides
//
// Design principles:
// - No exceptions (game console compatibility)
// - Case-by-case error handling based on severity
// - Retry on recoverable errors (network timeouts, etc.)
// - Let caller decide policy for non-fatal errors
// - This is NOT a gotcha - it's intentional design
////////////////////////////////////////////////////////////////////////////////

// Asset lifecycle states
enum class AssetState : ::ork::crc_enum_t {
  CrcEnum(NEW),             // brand new request
  CrcEnum(ENQUEUE_PENDING), // awaiting enqueue
  CrcEnum(ENQUEUED),        // Enqueued for download
  CrcEnum(DOWNLOADING),     // Currently downloading
  CrcEnum(PROCESSING),      // assembling, verifying, decrypting, etc.
  CrcEnum(SUCCEEDED),       // asset is ready
  CrcEnum(FAILED),          // asset has failed
};

// Recoverable errors - operations can retry or fallback
enum class AssetStatus : ::ork::crc_enum_t {
  CrcEnum(OK),              // Success
  // Network/IO errors
  CrcEnum(NETWORK),         // Network failure
  CrcEnum(DOWNLOAD_FAILED), // Download failed
  CrcEnum(UPLOAD_FAILED),   // Upload failed
  CrcEnum(FILE_NOT_FOUND),  // File doesn't exist
  CrcEnum(PERMISSION),      // Access denied
  CrcEnum(TIMEOUT),         // Operation timed out
  CrcEnum(CANCELLED),       // User cancelled
  // Asset-specific errors
  CrcEnum(NOT_FOUND),       // Asset not in manifest
  CrcEnum(CHECKSUM),        // Checksum mismatch
  CrcEnum(DECRYPT_FAILED),  // Decryption failed
  CrcEnum(DECOMPRESS_FAILED), // Decompression failed
  CrcEnum(UNSUPPORTED),     // Unsupported format/version
};

// Fatal errors - these will OrkAssert(false) in implementation
enum class AssetFatalError : ::ork::crc_enum_t {
  CrcEnum(INVALID_MANIFEST),   // Corrupt manifest structure
  CrcEnum(CODEC_INIT),         // Can't create encryption codec
  CrcEnum(OUT_OF_MEMORY),      // Memory allocation failed
  CrcEnum(CORRUPT_CACHE),      // Cache corruption
  CrcEnum(INVALID_CONFIG),     // Invalid configuration
};

// Simple result type for operations that can fail
template<typename T>
struct AssetOpResult {
  T value;
  AssetStatus error = AssetStatus::OK;
  std::string error_detail;  // Additional context
  
  bool isOk() const { return error == AssetStatus::OK; }
  operator bool() const { return isOk(); }
};

////////////////////////////////////////////////////////////////////////////////
// Callback types used throughout the asset catalog system
////////////////////////////////////////////////////////////////////////////////

// Core asset callbacks
using asset_callback_t = std::function<void(fetchrequest_ptr_t)>;
using asset_callback_list_t = std::vector<asset_callback_t>;
using download_progress_callback_t = std::function<void(const DownloadProgress&)>;
using completion_callback_t = std::function<void(datablock_ptr_t)>;
using error_callback_t = std::function<void(const std::string&)>;

// Upload callbacks
using upload_progress_callback_t = std::function<void(const UploadProgress&)>;
using aggregate_upload_progress_callback_t = std::function<void(
  const std::string& uploader_name,
  const UploadProgress& progress
)>;

// Packager callbacks
using packager_progress_callback_t = std::function<void(const std::string&, float)>;

// AssetRequest/Fetcher callbacks (legacy)
using asset_request_progress_fn_t = std::function<void(size_t downloaded, size_t total)>;
using asset_fetcher_progress_fn_t = std::function<void(const std::string&, size_t, size_t)>;
using asset_fetcher_complete_fn_t = std::function<void(const std::string&, bool)>;

////////////////////////////////////////////////////////////////////////////////
// Python-safe callback type aliases
// Using ItemAndData wrapper for GIL compatibility
// ALL ItemAndData types should be defined here for consistency
//
// GIL MANAGEMENT: Python GIL is properly managed in pyext binding layer:
// - py::gil_scoped_release used before blocking C++ operations
// - py::gil_scoped_acquire used before invoking Python callbacks
// - ItemAndData wrapper stores Python function objects safely
// - See pyext_asset_catalog.cpp, pyext_download.cpp for examples
// This is NOT a gotcha - proper thread safety is implemented
////////////////////////////////////////////////////////////////////////////////

// Core asset callbacks
using pysafe_asset_callback_t = ::ork::ItemAndData<asset_callback_t>;
using pysafe_download_progress_callback_t = ::ork::ItemAndData<download_progress_callback_t>;
using pysafe_completion_callback_t = ::ork::ItemAndData<completion_callback_t>;
using pysafe_error_callback_t = ::ork::ItemAndData<error_callback_t>;

// Upload callbacks
using pysafe_upload_progress_callback_t = ::ork::ItemAndData<upload_progress_callback_t>;
using pysafe_aggregate_upload_progress_t = ::ork::ItemAndData<aggregate_upload_progress_callback_t>;

// Packager callbacks
using pysafe_packager_progress_t = ::ork::ItemAndData<packager_progress_callback_t>;

// AssetRequest/Fetcher callbacks
using pysafe_asset_request_progress_t = ::ork::ItemAndData<asset_request_progress_fn_t>;
using pysafe_asset_fetcher_progress_t = ::ork::ItemAndData<asset_fetcher_progress_fn_t>;
using pysafe_asset_fetcher_complete_t = ::ork::ItemAndData<asset_fetcher_complete_fn_t>;

// Asset catalog progress callbacks
using asset_progress_callback_t = std::function<void(const std::string& asset_id, size_t downloaded, size_t total)>;
using pysafe_asset_progress_callback_t = ::ork::ItemAndData<asset_progress_callback_t>;

// Preload callbacks
using preload_progress_callback_t = std::function<void(const std::string& asset_id, bool success)>;
using preload_complete_callback_t = std::function<void(size_t succeeded, size_t failed)>;
using pysafe_preload_progress_callback_t = ::ork::ItemAndData<preload_progress_callback_t>;
using pysafe_preload_complete_callback_t = ::ork::ItemAndData<preload_complete_callback_t>;

} // namespace ork::asset::catalog
