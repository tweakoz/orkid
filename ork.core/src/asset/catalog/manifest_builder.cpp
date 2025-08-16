////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/manifest_builder.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////
// ManifestBuilderImpl - Pimpl implementation
////////////////////////////////////////////////////////////////

struct ManifestBuilderImpl {
  ManifestBuilder* _builder;
  manifestbuildconfig_ptr_t _config;
  manifest_entry_list_t _entries;
  progress_callback_fn_t _progress_callback;
  filter_callback_fn_t _file_filter;
  
  // Cache for file operations
  mutable LockedResource<hash_cache_map_t> _hash_cache;
  
  ManifestBuilderImpl(ManifestBuilder* builder, manifestbuildconfig_ptr_t config)
      : _builder(builder), _config(config) {}
  
  // Internal methods
  void scanDirectory(
    const file::Path& dir,
    const file::Path& root_dir,
    std::vector<file::Path>& files
  );
  
  assetentry_ptr_t processFile(
    const file::Path& file_path,
    const file::Path& root_dir
  );
  
  bool shouldIncludeFile(const file::Path& path) const;
  uint64_t computeFileHash(const file::Path& path) const;
  bool isFileCompressed(const file::Path& path) const;
  std::string buildStorageUrl(const std::string& base_url, 
                             const std::string& hash,
                             bool is_encrypted = false) const;
};

// ManifestEntry methods removed - now using AssetEntry directly

////////////////////////////////////////////////////////////////
// ManifestBuildConfig
////////////////////////////////////////////////////////////////

void ManifestBuildConfig::addIncludePattern(const std::string& pattern) {
  include_patterns.push_back(pattern);
}

void ManifestBuildConfig::addExcludePattern(const std::string& pattern) {
  exclude_patterns.push_back(pattern);
}

void ManifestBuildConfig::addIncludeRegex(const std::string& pattern) {
  include_regex.push_back(std::regex(pattern));
}

void ManifestBuildConfig::addExcludeRegex(const std::string& pattern) {
  exclude_regex.push_back(std::regex(pattern));
}

bool ManifestBuildConfig::isValid() const {
  return !namespace_id.empty();
}

std::string ManifestBuildConfig::getValidationError() const {
  if (namespace_id.empty()) return "Namespace ID is required";
  return "";
}

////////////////////////////////////////////////////////////////
// ManifestBuilder
////////////////////////////////////////////////////////////////

ManifestBuilder::ManifestBuilder(manifestbuildconfig_ptr_t config) {
  auto impl = _impl.makeShared<ManifestBuilderImpl>(this, config);
}

ManifestBuilder::~ManifestBuilder() {
}

manifestbuilder_ptr_t ManifestBuilder::create(manifestbuildconfig_ptr_t config) {
  if (!config || !config->isValid()) {
    return nullptr;
  }
  return std::make_shared<ManifestBuilder>(config);
}

const manifest_entry_list_t& ManifestBuilder::getEntries() const {
  auto impl = _impl.getShared<ManifestBuilderImpl>();
  return impl->_entries;
}

void ManifestBuilder::setProgressCallback(progress_callback_fn_t callback) {
  auto impl = _impl.getShared<ManifestBuilderImpl>();
  impl->_progress_callback = callback;
}

void ManifestBuilder::setFileFilter(filter_callback_fn_t filter) {
  auto impl = _impl.getShared<ManifestBuilderImpl>();
  impl->_file_filter = filter;
}

assetmanifest_ptr_t ManifestBuilder::buildFromDirectory(const file::Path& root_dir) {
  auto impl = _impl.getShared<ManifestBuilderImpl>();
  
  // Clear existing entries
  impl->_entries.clear();
  
  // Scan directory for files
  std::vector<file::Path> files;
  impl->scanDirectory(root_dir, root_dir, files);
  
  // Process each file
  size_t total = files.size();
  for (size_t i = 0; i < files.size(); ++i) {
    // Progress callback
    if (impl->_progress_callback) {
      impl->_progress_callback(files[i].c_str(), i + 1, total);
    }
    
    // Process file
    auto entry = impl->processFile(files[i], root_dir);
    if (entry) {
      impl->_entries.push_back(entry);
    }
  }
  
  // Create manifest
  auto manifest = std::make_shared<AssetManifest>();
  manifest->setNamespace(impl->_config->namespace_id);
  manifest->setVersion(impl->_config->version);
  
  // Add entries to manifest
  for (const auto& entry : impl->_entries) {
    if (entry) {
      // Use relative path as the key
      manifest->addAsset(entry->_relative_path, entry);
    }
  }
  
  return manifest;
}

assetmanifest_ptr_t ManifestBuilder::buildFromFileList(
    const file::Path& root_dir,
    const std::vector<file::Path>& files) {
  auto impl = _impl.getShared<ManifestBuilderImpl>();
  
  // Clear existing entries
  impl->_entries.clear();
  
  // Process each file
  size_t total = files.size();
  for (size_t i = 0; i < files.size(); ++i) {
    // Progress callback
    if (impl->_progress_callback) {
      impl->_progress_callback(files[i].c_str(), i + 1, total);
    }
    
    // Process file
    auto entry = impl->processFile(files[i], root_dir);
    if (entry) {
      impl->_entries.push_back(entry);
    }
  }
  
  // Create manifest
  auto manifest = std::make_shared<AssetManifest>();
  manifest->setNamespace(impl->_config->namespace_id);
  manifest->setVersion(impl->_config->version);
  
  // Add entries to manifest
  for (const auto& entry : impl->_entries) {
    if (entry) {
      // Use relative path as the key
      manifest->addAsset(entry->_relative_path, entry);
    }
  }
  
  return manifest;
}

assetmanifest_ptr_t ManifestBuilder::updateManifest(
    assetmanifest_ptr_t existing,
    const file::Path& root_dir,
    bool remove_deleted) {
  auto impl = _impl.getShared<ManifestBuilderImpl>();
  // TODO: Implement manifest updating
  return nullptr;
}

assetmanifest_ptr_t ManifestBuilder::mergeManifests(
    const std::vector<assetmanifest_ptr_t>& manifests) {
  // TODO: Implement manifest merging
  return nullptr;
}

void ManifestBuilderImpl::scanDirectory(
    const file::Path& dir,
    const file::Path& root_dir,
    std::vector<file::Path>& files) {
  // TODO: Implement recursive directory scanning
  // Check if should follow symlinks from config
  // Apply include/exclude patterns
  // Add matching files to the files vector
}

assetentry_ptr_t ManifestBuilderImpl::processFile(
    const file::Path& file_path,
    const file::Path& root_dir) {
  // Check if file should be included
  if (!shouldIncludeFile(file_path)) {
    return nullptr;
  }
  
  // Check file filter callback
  if (_file_filter && !_file_filter(file_path)) {
    return nullptr;
  }
  
  auto entry = std::make_shared<AssetEntry>();
  
  // Basic info
  // filename field no longer used
  // TODO: Implement relative path calculation
  entry->_relative_path = file_path.c_str(); // Temporary
  entry->_namespace = _config->namespace_id;
  
  // Get file info
  // TODO: Get file size, modification time
  
  // Compute hash if enabled
  if (_config->compute_hashes) {
    uint64_t hash_value = computeFileHash(file_path);
    // For manifest builder, we're not computing MD5 hashes
    // This would need to be done by the packager
    entry->_storage_hash = std::to_string(hash_value);
    entry->_hash_algorithm = "xxhash64"; // Temporary - should be md5
  }
  
  // Detect compression
  if (_config->detect_compression) {
    entry->_is_compressed = isFileCompressed(file_path);
  }
  
  // Build content URLs
  if (!_config->base_url.empty() && !entry->_storage_hash.empty()) {
    entry->_local_loc = buildStorageUrl(_config->base_url, entry->_storage_hash);
  }
  if (!_config->source_url.empty() && !entry->_storage_hash.empty()) {
    entry->_remote_loc = buildStorageUrl(_config->source_url, entry->_storage_hash);
  }
  
  // Platform info
  entry->_platforms = _config->target_platforms;
  
  return entry;
}

bool ManifestBuilderImpl::shouldIncludeFile(const file::Path& path) const {
  std::string filename = path.c_str();
  
  // Check exclude patterns first
  for (const auto& pattern : _config->exclude_patterns) {
    std::regex regex = globToRegex(pattern);
    if (std::regex_match(filename, regex)) {
      return false;
    }
  }
  
  // Check exclude regex
  for (const auto& regex : _config->exclude_regex) {
    if (std::regex_match(filename, regex)) {
      return false;
    }
  }
  
  // Check include patterns
  bool included = false;
  if (_config->include_patterns.empty() && _config->include_regex.empty()) {
    included = true; // Include all if no patterns specified
  } else {
    // Check glob patterns
    for (const auto& pattern : _config->include_patterns) {
      std::regex regex = globToRegex(pattern);
      if (std::regex_match(filename, regex)) {
        included = true;
        break;
      }
    }
    
    // Check regex patterns
    if (!included) {
      for (const auto& regex : _config->include_regex) {
        if (std::regex_match(filename, regex)) {
          included = true;
          break;
        }
      }
    }
  }
  
  return included;
}

uint64_t ManifestBuilderImpl::computeFileHash(const file::Path& path) const {
  // Check cache first
  std::string path_str = path.c_str();
  uint64_t cached_hash = 0;
  
  _hash_cache.atomicOp([&path_str, &cached_hash](const hash_cache_map_t& cache) {
    auto it = cache.find(path_str);
    if (it != cache.end()) {
      cached_hash = it->second;
    }
  });
  
  if (cached_hash != 0) {
    return cached_hash;
  }
  
  // TODO: Implement actual file hashing using xxhash64
  // For now, return a placeholder
  uint64_t hash = 0;
  
  // Cache the result
  if (hash != 0) {
    _hash_cache.atomicOp([&path_str, hash](hash_cache_map_t& cache) {
      cache[path_str] = hash;
    });
  }
  
  return hash;
}

bool ManifestBuilderImpl::isFileCompressed(const file::Path& path) const {
  std::string ext = getFileExtension(path);
  return isCompressedFileExtension(ext);
}

std::string ManifestBuilderImpl::buildStorageUrl(const std::string& base_url, 
                                             const std::string& hash,
                                             bool is_encrypted) const {
  std::string url = base_url;
  if (!url.empty() && url.back() != '/') {
    url += '/';
  }
  url += hash;
  if (is_encrypted) {
    url += ".enc";
  }
  return url;
}

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

std::regex globToRegex(const std::string& glob) {
  std::string regex_str;
  for (char c : glob) {
    switch (c) {
      case '*': regex_str += ".*"; break;
      case '?': regex_str += "."; break;
      case '.': regex_str += "\\."; break;
      case '\\': regex_str += "\\\\"; break;
      default: regex_str += c; break;
    }
  }
  return std::regex(regex_str);
}

std::string getFileExtension(const file::Path& path) {
  std::string filename = path.c_str();
  size_t dot_pos = filename.find_last_of('.');
  if (dot_pos != std::string::npos) {
    std::string ext = filename.substr(dot_pos + 1);
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
  }
  return "";
}

bool isCompressedFileExtension(const std::string& ext) {
  static const std::set<std::string> compressed_exts = {
    "gz", "bz2", "xz", "zip", "rar", "7z", "tar",
    "tgz", "tbz2", "txz", "lz4", "zst", "z"
  };
  return compressed_exts.find(ext) != compressed_exts.end();
}

} // namespace ork::asset::catalog