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
#include <ork/kernel/svariant.h>
#include <ork/kernel/mutex.h>
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/chunk_manifest.h>
#include <map>
#include <string>
#include <memory>
#include <functional>

namespace ork::asset::catalog {

// Forward declarations
struct AssetCatalog;

// Type aliases moved to types.h

////////////////////////////////////////////////////////////////////////////////
// Asset Entry - represents a single asset in a manifest
////////////////////////////////////////////////////////////////////////////////

struct AssetEntry {
  // Basic info
  std::string _id;                       // Asset ID within namespace (populated when parsing)
  std::string _type = "asset";           // "asset_pak" or "asset"
  int _priority = 100;
  bool _merge = false;
  std::string _local_loc;                // Local location - where to extract/find files
                                         // For paks: extraction destination
                                         // For assets: file location
  std::string _relative_path;            // Full relative path within namespace
  std::string _tar_root;                 // Root directory in TAR for asset_pak (can be empty)
  std::vector<std::string> _filters;     // File patterns to include in TAR (empty = include all)
  
  // Extended metadata
  std::string _storage_hash;             // Storage hash (encrypted data) - for CAFS naming
  std::string _content_hash;             // Content hash (raw data) - for verification
  std::string _hash_algorithm = "md5";   // Hash algorithm used
  time_t _modification_time = 0;         // Last modification time
  
  // Compression/encryption info
  CompressionType _compression_type = CompressionType::LZ4;
  size_t _archive_size = 0;             // Size before compression and encryption
  size_t _encrypted_size = 0;           // Size after encryption
  size_t _compressed_size = 0;          // Size after compression

  // Chunk info for large files
  chunkmanifest_ptr_t _chunk_manifest;
  
  // Platform/dependency info
  platform_list_t _platforms;            // Supported platforms: ["mac"], ["linux"], ["mac", "linux"]
  asset_dependency_map_t _dependencies;
  
  // Source tracking
  std::string _manifest_source;          // Which manifest file this came from
  namespaceid_t _namespace;              // Namespace this asset belongs to
  std::weak_ptr<AssetNamespace> _namespace_ptr; // Weak pointer to namespace object
  assetmanifest_wkptr_t _parent_manifest; // Weak pointer to parent manifest
  
  // Check if this asset supports the current platform
  bool supportsCurrentPlatform() const;
  
  // Build fully qualified asset ID
  // Returns: {_namespace}::{_id}
  std::string buildFullyQualifiedId() const;
    
  // Repackage asset (recompute hashes, rechunk if needed)
  void repackage();
  datablock_ptr_t _archiveAsset(assetfqid_ptr_t fqid); 
  datablock_ptr_t _encryptAsset(datablock_ptr_t raw_data); 
  
  // Upload asset file to configured remote location
  // Returns: upload receipt with results
  uploadreceipt_ptr_t upload(
    const AssetConfig& config,
    locationinfo_ptr_t location,
    chunk_completed_callback_t on_chunk_completed = nullptr) const;
  
  // Get resolved local path (with templates expanded)
  // Returns: local path with <stage>, <cache>, etc. resolved
  file::Path getResolvedLocalPath() const;
  
  // Get path to local encrypted file (after repackaging)
  // Returns: {resolved_local_path}/{storage_hash}.enc
  file::Path getLocalEncryptedPath() const;
  
  // Get parent manifest
  assetmanifest_ptr_t getParentManifest() const;
  
  // Check if asset has been repackaged
  bool isRepackaged() const;
  
  // Get catalog from parent manifest
  assetcatalog_ptr_t getCatalog() const;
  
  private:
  // Helper methods for upload
  void saveReceipt(uploadreceipt_ptr_t receipt) const;
  std::string formatChunkIndex(int index) const;
  
};

////////////////////////////////////////////////////////////////////////////////

struct AssetManifest {
  
  //////////////////////////////////////////////////////////////////////////////
  // Constructor/Destructor
  //////////////////////////////////////////////////////////////////////////////
  AssetManifest();
  AssetManifest(assetcatalog_wkptr_t parent_catalog);
  ~AssetManifest();
  
  assetcatalog_wkptr_t _parent_catalog;

  //////////////////////////////////////////////////////////////////////////////
  // Methods
  //////////////////////////////////////////////////////////////////////////////
  
  assetconfig_ptr_t getConfig() const;

  // Load from JSON file
  static assetmanifest_ptr_t loadFromFile(const file::Path& path);
  
  // Parse from JSON string
  static assetmanifest_ptr_t parseFromString(const std::string& json_str, const file::Path& source_file);
  
  // Save to JSON file
  bool saveToFile(const file::Path& path) const;
    
  // Convert to JSON string with pretty formatting
  std::string toJson() const;
    
  // Merge another manifest into this one
  void merge(const AssetManifest& other);
  
  // Get total size of all assets
  size_t getTotalSize() const;
  
  // Get total compressed size
  size_t getTotalCompressedSize() const;
  
  // Count assets by type
  asset_type_count_map_t countAssetsByType() const;
    
  // Accessors for pimpl
  const std::string& getManifestId() const;
  const namespaceid_t& getNamespace() const;
  const std::string& getVersion() const;
  const std::string& getBaseUrl() const;
  const asset_entry_map_t& getAssets() const;
  const asset_metadata_map_t& getMetadata() const;
  const file::Path& getSourceFile() const;
  
  // Get codec for this manifest's namespace
  encryptioncodec_ptr_t getCodec() const;
  
  void setNamespace(const namespaceid_t& ns);
  void setVersion(const std::string& version);
  void setBaseUrl(const std::string& url);
  void setDescription(const std::string& desc);
  void setCreator(const std::string& creator);
  void setCreationTime(time_t time);
  
  // Direct asset manipulation for builders
  void addAsset(const assetid_t& id, assetentry_ptr_t entry);
  
  // Create asset with builder pattern (static factory)
  static assetentry_ptr_t createAsset(
    assetmanifest_ptr_t self,
    const assetid_t& id,
    int priority,
    const std::string& type,
    const std::string& local,
    const platform_list_t& platforms,
    const assetid_list_t& dependencies,
    const std::string& tar_root = "",
    const std::vector<std::string>& filters = {});
  
  // Repackage all assets in manifest
  void repackage();
  
  // Upload all assets in manifest to configured remote location
  // Returns: upload receipt with results
  uploadreceipt_ptr_t upload(
    const AssetConfig& config,
    locationinfo_ptr_t location,
    asset_completed_callback_t on_asset_completed = nullptr) const;
  
  // Get parent catalog (for accessing cache directory, etc)
  assetcatalog_ptr_t getParentCatalog() const;
  
private:
  // Implementation
  svar64_t _impl;
  
  // Internal parsing
  void parseFromJsonInternal(const std::string& json_str, const file::Path& source_file);
};

////////////////////////////////////////////////////////////////////////////////
// LocalManifest - represents metadata of an asset stored locally 
//  (not in catalog)
////////////////////////////////////////////////////////////////////////////////

struct LocalManifest {
  file::Path _path; 
  std::string _fqid;
  std::string _type = "asset_pak";
  size_t _archive_size = 0;
  size_t _encrypted_size = 0;
  size_t _compressed_size = 0;
  std::string _unwrapped_path;
  file::Path _encrypted_path; 
  std::string _storage_hash;
  std::string _content_hash;
  std::string _timestamp;
  bool _auto_unwrap = false;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog