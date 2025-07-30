////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/chunk_manifest.h>
#include <ork/file/path.h>
#include <ork/kernel/mutex.h>
#include <functional>
#include <regex>

namespace ork::asset::catalog {

// Type aliases moved to types.h

// ManifestEntry has been merged into AssetEntry in manifest.h
// The ManifestBuilder now uses AssetEntry directly

////////////////////////////////////////////////////////////////////////////////
// Configuration for manifest building
////////////////////////////////////////////////////////////////////////////////

struct ManifestBuildConfig {
  // Namespace info
  std::string namespace_id;             // Required: namespace for this manifest
  std::string version = "1.0.0";        // Version of this manifest
  
  // File selection
  pattern_list_t include_patterns = {"*"};  // Glob patterns to include
  pattern_list_t exclude_patterns;          // Glob patterns to exclude
  regex_list_t include_regex;              // Regex patterns to include
  regex_list_t exclude_regex;              // Regex patterns to exclude
  
  // Platform filtering
  platform_list_t target_platforms;         // If empty, all platforms
  
  // Metadata options
  bool compute_hashes = true;           // Calculate content hashes
  bool detect_compression = true;       // Auto-detect if files are compressed
  bool include_chunk_info = true;       // Include chunking info for large files
  size_t chunk_threshold = 10 * 1024 * 1024;  // Files larger than this get chunk info
  
  // Base URLs for content-addressable storage
  std::string base_url = "";            // e.g., "https://cdn.example.com/assets"
  std::string source_url = "";          // e.g., "https://source.example.com/assets"
  
  // Processing options
  bool follow_symlinks = false;
  bool verbose = false;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Add file pattern helpers
  ////////////////////////////////////////////////////////////////////////////////
  void addIncludePattern(const std::string& pattern);
  void addExcludePattern(const std::string& pattern);
  void addIncludeRegex(const std::string& pattern);
  void addExcludeRegex(const std::string& pattern);
  
  ////////////////////////////////////////////////////////////////////////////////
  // Validation
  ////////////////////////////////////////////////////////////////////////////////
  bool isValid() const;
  std::string getValidationError() const;
};

////////////////////////////////////////////////////////////////////////////////
// Manifest builder - scans directories and generates manifest files
//
// Responsible for creating asset manifests during the packaging phase:
// - Recursively scans directories for assets
// - Applies include/exclude patterns (glob and regex)
// - Computes file hashes (MD5 by default, BLAKE2b optional)
// - Detects already-compressed files to avoid double compression
// - Generates chunk info for large files (> threshold)
// - Creates ManifestEntry with all metadata needed by catalog
//
// Works in conjunction with AssetPackager:
// 1. ManifestBuilder scans and creates manifest entries
// 2. AssetPackager processes files (compress/encrypt/chunk)
// 3. Final manifest includes all metadata for retrieval
////////////////////////////////////////////////////////////////////////////////

struct ManifestBuilder {
  ////////////////////////////////////////////////////////////////////////////////
  // Factory method
  ////////////////////////////////////////////////////////////////////////////////
  static manifestbuilder_ptr_t create(manifestbuildconfig_ptr_t config);
  
  ~ManifestBuilder();
  
  ////////////////////////////////////////////////////////////////////////////////
  // Build manifest from a directory
  // Returns the generated manifest
  ////////////////////////////////////////////////////////////////////////////////
  assetmanifest_ptr_t buildFromDirectory(const file::Path& root_dir);
  
  ////////////////////////////////////////////////////////////////////////////////
  // Build manifest from a list of files
  // Useful when you have a specific file list
  ////////////////////////////////////////////////////////////////////////////////
  assetmanifest_ptr_t buildFromFileList(
    const file::Path& root_dir,
    const std::vector<file::Path>& files
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Update an existing manifest
  // - Adds new files
  // - Updates changed files
  // - Optionally removes deleted files
  ////////////////////////////////////////////////////////////////////////////////
  assetmanifest_ptr_t updateManifest(
    assetmanifest_ptr_t existing,
    const file::Path& root_dir,
    bool remove_deleted = true
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Merge multiple manifests
  // Uses priority field to resolve conflicts
  ////////////////////////////////////////////////////////////////////////////////
  static assetmanifest_ptr_t mergeManifests(
    const std::vector<assetmanifest_ptr_t>& manifests
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress callback
  // Called as files are processed
  ////////////////////////////////////////////////////////////////////////////////
  
  ////////////////////////////////////////////////////////////////////////////////
  // File filter callback
  // Return false to skip a file
  ////////////////////////////////////////////////////////////////////////////////
  
  ////////////////////////////////////////////////////////////////////////////////
  // Get list of all entries built
  ////////////////////////////////////////////////////////////////////////////////
  const manifest_entry_list_t& getEntries() const;
  
  // Set callbacks
  void setProgressCallback(progress_callback_fn_t callback);
  void setFileFilter(filter_callback_fn_t filter);
  
  // Constructor
  ManifestBuilder(manifestbuildconfig_ptr_t config);
  
private:
  // Implementation
  svar64_t _impl;
};

////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////

// Convert glob pattern to regex
std::regex globToRegex(const std::string& glob);

// Get file extension in lowercase
std::string getFileExtension(const file::Path& path);

// Check if file extension indicates compression
bool isCompressedFileExtension(const std::string& ext);

} // namespace ork::asset::catalog