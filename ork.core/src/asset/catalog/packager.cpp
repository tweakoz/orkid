////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/packager.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/mutex.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <sstream>

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");


////////////////////////////////////////////////////////////////
// AssetPackagerImpl - Pimpl implementation
////////////////////////////////////////////////////////////////

struct AssetPackagerImpl {
  AssetPackager* _packager;
  packageconfig_ptr_t _config;
  encryptioncodec_ptr_t _codec;      // From catalog hierarchy
  std::atomic<bool> _cancelled{false};
  pysafe_packager_progress_t _progress_callback;  // Python-bindable
  
  // Thread-safe statistics
  std::atomic<size_t> _processed_count{0};
  std::atomic<size_t> _total_count{0};
  
  AssetPackagerImpl(AssetPackager* packager, packageconfig_ptr_t config, encryptioncodec_ptr_t codec)
      : _packager(packager), _config(config), _codec(codec) {}
  
  // Internal methods
  AssetPackageResult processFile(
    const file::Path& input_file,
    const file::Path& output_base
  );
  
  AssetPackageResult processChunkedFile(
    const file::Path& input_file,
    const file::Path& output_base,
    chunkmanifest_ptr_t chunk_manifest
  );
  
  bool processChunk(
    const datablock_ptr_t& chunk_data,
    size_t chunk_index,
    const file::Path& output_path,
    ChunkMeta& chunk_meta
  );
  
  datablock_ptr_t compressData(const datablock_ptr_t& _data);
  datablock_ptr_t encryptData(const datablock_ptr_t& _data);
};

////////////////////////////////////////////////////////////////
// PackageConfig
////////////////////////////////////////////////////////////////

bool PackageConfig::isValid() const {
  if (output_dir.empty()) {
    return false;
  }
  
  if (enable_encryption && namespace_id.empty()) {
    return false;
  }
  
  if (chunk_size == 0 || chunk_threshold == 0) {
    return false;
  }
  
  return true;
}

std::string PackageConfig::getValidationError() const {
  if (output_dir.empty()) {
    return "Output directory is required";
  }
  
  if (enable_encryption && namespace_id.empty()) {
    return "Namespace ID is required for encryption";
  }
  
  if (chunk_size == 0) {
    return "Chunk size must be greater than 0";
  }
  
  if (chunk_threshold == 0) {
    return "Chunk threshold must be greater than 0";
  }
  
  return "";
}

////////////////////////////////////////////////////////////////
// PackageResult
////////////////////////////////////////////////////////////////

std::string PackageResult::getSummary() const {
  std::stringstream ss;
  ss << "Packaging Summary:\n";
  ss << "  Total Assets: " << total_assets << "\n";
  ss << "  Successful: " << successful_assets << "\n";
  ss << "  Failed: " << failed_assets << "\n";
  ss << "  Original Size: " << (total_original_size / (1024 * 1024)) << " MB\n";
  ss << "  Compressed Size: " << (total_compressed_size / (1024 * 1024)) << " MB\n";
  if (total_original_size > 0) {
    float ratio = float(total_compressed_size) / float(total_original_size);
    ss << "  Compression Ratio: " << (ratio * 100.0f) << "%\n";
  }
  ss << "  Total Time: " << total_time << " seconds\n";
  return ss.str();
}

failed_asset_list_t PackageResult::getFailedAssets() const {
  failed_asset_list_t failed;
  for (const auto& result : asset_results) {
    if (!result.success) {
      failed.push_back(result.asset_path);
    }
  }
  return failed;
}

////////////////////////////////////////////////////////////////
// AssetPackager
////////////////////////////////////////////////////////////////

AssetPackager::AssetPackager(
    packageconfig_ptr_t config,
    encryptioncodec_ptr_t codec) {
  _impl.makeShared<AssetPackagerImpl>(this, config, codec);
}

AssetPackager::~AssetPackager() {
}

assetpackager_ptr_t AssetPackager::create(
    packageconfig_ptr_t config,
    assetcatalog_ptr_t catalog) {
  
  if (!config || !config->isValid()) {
    return nullptr;
  }
  
  // Get codec from catalog if encryption is enabled
  encryptioncodec_ptr_t codec;
  if (config->enable_encryption && catalog) {
    // Find the namespace and get its codec
    auto ns = catalog->findNamespace(config->namespace_id);
    if (ns) {
      codec = ns->getCodec();
    } else {
      logchan_catalog->log("WARNING: Namespace '%s' not found, encryption will be disabled", 
                           config->namespace_id.c_str());
    }
  }
  
  return assetpackager_ptr_t(new AssetPackager(config, codec));
}

void AssetPackager::cancel() {
  auto impl = _impl.getShared<AssetPackagerImpl>();
  impl->_cancelled = true;
}

bool AssetPackager::isCancelled() const {
  auto impl = _impl.getShared<AssetPackagerImpl>();
  return impl->_cancelled.load();
}

void AssetPackager::setProgressCallback(pysafe_packager_progress_t callback) {
  auto impl = _impl.getShared<AssetPackagerImpl>();
  impl->_progress_callback = callback;
}

packageresult_ptr_t AssetPackager::packageDirectory(
    const file::Path& source_dir,
    const pattern_list_t& file_patterns) {
  auto impl = _impl.getShared<AssetPackagerImpl>();
  
  auto result = std::make_shared<PackageResult>();
  
  // TODO: Implement directory scanning and packaging
  // 1. Scan directory for files matching patterns
  // 2. Queue each file for processing
  // 3. Process files in parallel using work_queue
  // 4. Generate manifest if requested
  
  return result;
}

AssetPackageResult AssetPackager::packageFile(
    const file::Path& source_file,
    const file::Path& relative_path) {
  auto impl = _impl.getShared<AssetPackagerImpl>();
  
  // Determine output base path
  file::Path output_base = impl->_config->output_dir / relative_path;
  
  return impl->processFile(source_file, output_base);
}

////////////////////////////////////////////////////////////////
// AssetPackagerImpl methods
////////////////////////////////////////////////////////////////

AssetPackageResult AssetPackagerImpl::processFile(
    const file::Path& input_file,
    const file::Path& output_base) {
  
  AssetPackageResult result;
  result.asset_path = input_file.c_str();
  
  Timer timer;
  timer.Start();
  
  try {
    // 1. Read file
    auto _data = std::make_shared<DataBlock>();
    if (!FileEnv::GetRef().DoesFileExist(input_file)) {
      result.success = false;
      result.error_message = FormatString("File does not exist: %s", input_file.c_str());
      return result;
    }
    
    // Initialize MD5 hasher
    CMD5 file_hasher;
    
    File inputFile(input_file, EFM_READ);
    size_t fileSize = 0;
    inputFile.GetLength(fileSize);
    result.original_size = fileSize;
    
    if (fileSize > 0) {
      _data->reserve(fileSize);
      _data->_storage.resize(fileSize);
      inputFile.Read(const_cast<uint8_t*>(_data->data()), fileSize);
      
      // Calculate MD5 hash of the raw file data
      file_hasher.update(_data->data(), fileSize);
      file_hasher.finalize();
    }
    
    // 2. Check if should chunk
    if (shouldChunkFile(input_file, *_config)) {
      // Handle as chunked file
      auto chunk_manifest = std::make_shared<ChunkManifest>();
      return processChunkedFile(input_file, output_base, chunk_manifest);
    }
    
    // 3. Compress
    auto compressed = compressData(_data);
    result.compressed_size = compressed->length();
    result.compression_ratio = calculateCompressionRatio(result.original_size, result.compressed_size);
    
    // 4. Encrypt if enabled
    auto encrypted = encryptData(compressed);
    
    // 5. Calculate both hashes
    // Content hash - MD5 of the original file
    Md5Sum content_md5_result = file_hasher.Result();
    result.content_hash = content_md5_result.hex_digest();
    
    // Storage hash - MD5 of the encrypted _data (for CAFS naming)
    CMD5 storage_hasher;
    storage_hasher.update(encrypted->data(), encrypted->length());
    storage_hasher.finalize();
    Md5Sum storage_md5_result = storage_hasher.Result();
    result.storage_hash = storage_md5_result.hex_digest();
    
    // 6. Write output
    // For content-addressable storage: output is {hash}.enc or just {hash}
    // Only add .enc extension if encryption was actually applied
    bool was_encrypted = _config->enable_encryption && _codec;
    file::Path output_path = output_base.toAbsolute() / FormatString("%s%s", 
      result.storage_hash.c_str(), 
      was_encrypted ? ".enc" : "");
    
    // Create output directory if it doesn't exist
    output_base.ensureDirectoryExists();
    
    // Write file
    File outputFile(output_path, EFM_WRITE);
    outputFile.Write(encrypted->data(), encrypted->length());
    
    result.success = true;
    result.processing_time = timer.SecsSinceStart();
    
    // Report progress if callback set
    if (_progress_callback._item) {
      _processed_count++;
      std::string progress_msg = FormatString("Processing %s (%zu/%zu)", 
        result.asset_path.c_str(), 
        _processed_count.load(), 
        _total_count.load());
      float progress = _total_count > 0 ? float(_processed_count) / float(_total_count) : 0.0f;
      _progress_callback._item(progress_msg, progress);
    }
    
  } catch (const std::exception& e) {
    result.success = false;
    result.error_message = FormatString("Exception processing file: %s", e.what());
  }
  
  return result;
}

AssetPackageResult AssetPackagerImpl::processChunkedFile(
    const file::Path& input_file,
    const file::Path& output_base,
    chunkmanifest_ptr_t chunk_manifest) {
  
  AssetPackageResult result;
  result.asset_path = input_file.c_str();
  
  Timer timer;
  timer.Start();
  
  try {
    // Open input file
    File inputFile(input_file, EFM_READ);
    size_t fileSize = 0;
    inputFile.GetLength(fileSize);
    result.original_size = fileSize;
    
    if (fileSize == 0) {
      result.success = true;
      result.compressed_size = 0;
      return result;
    }
    
    constexpr size_t chunk_size = ChunkManifest::chunk_size; 
    // Initialize chunk manifest
    chunk_manifest->_total_size = fileSize;
    chunk_manifest->_file_hash = 0; // Will be calculated at the end
    chunk_manifest->_compression = _config->compression_type;
    chunk_manifest->_is_encrypted = _config->enable_encryption && _codec;
    size_t _total_chunks = (fileSize + chunk_size - 1) / chunk_size;
    
    // Initialize streaming MD5 hash for the entire file
    CMD5 file_hasher;
    
    // For storage hash of chunked file, we'll accumulate chunk hashes
    CMD5 storage_hasher;
    
    // Initialize XXHash for the chunk manifest file_hash
    auto file_xxhasher = std::make_shared<XXH64HASH>();
    file_xxhasher->init();
    
    // Process each chunk
    size_t bytes_processed = 0;
    size_t chunk_index = 0;
    size_t total_compressed_size = 0;
    
    while (bytes_processed < fileSize) {
      size_t chunk_data_size = std::min(chunk_size, fileSize - bytes_processed);
      
      // Read chunk data
      auto chunk_data = std::make_shared<DataBlock>();
      chunk_data->reserve(chunk_data_size);
      chunk_data->_storage.resize(chunk_data_size);
      inputFile.Read(const_cast<uint8_t*>(chunk_data->data()), chunk_data_size);
      
      // Update the streaming hash with raw file data
      file_hasher.update(chunk_data->data(), chunk_data_size);
      
      // Update XXHash with raw chunk _data for chunk manifest
      file_xxhasher->accumulate(chunk_data->data(), chunk_data_size);
      
      // Process chunk (compress, encrypt, write)
      ChunkMeta chunk_meta;
      chunk_meta._offset = bytes_processed;
      chunk_meta._size = chunk_data_size;
      
      // Compress chunk
      auto compressed = compressData(chunk_data);
      chunk_meta._compressed_size = compressed->length();
      
      // Encrypt chunk if enabled
      auto encrypted = encryptData(compressed);
      
      // Calculate chunk hash using XXHash64
      auto xxhasher = std::make_shared<XXH64HASH>();
      xxhasher->init();
      xxhasher->accumulate(encrypted->data(), encrypted->length());
      xxhasher->finish();
      chunk_meta._hash = xxhasher->result();
      
      // Update storage hash with encrypted chunk data
      storage_hasher.update(encrypted->data(), encrypted->length());
      
      // Determine output path for chunk
      // Use source filename as base for chunk naming during packaging
      // The final naming for upload/download will be: {file_hash}.enc.chunk.{index:04d}
      // But we don't know file_hash yet, so use source filename for now
      //
      // Packager output: {output_dir}/{source_filename}.chunk.{index:04d}
      // Download expects: {remote_loc}/{file_hash}.enc.chunk.{index:04d}
      //
      // The manifest builder or upload process must handle the final naming
      bool was_encrypted = _config->enable_encryption && _codec;
      // Get filename without extension for chunk naming
      std::string filename = input_file.getName().c_str();
      std::string ext = input_file.getExtension().c_str();
      std::string base_name = filename;
      if (!ext.empty() && filename.length() > ext.length() + 1) {
        // Remove extension (including the dot)
        base_name = filename.substr(0, filename.length() - ext.length() - 1);
      }
      file::Path chunk_path = output_base / FormatString("%s%s.chunk.%04zu", 
        base_name.c_str(),
        was_encrypted ? ".enc" : "",
        chunk_index);
      
      // Ensure output directory exists
      output_base.ensureDirectoryExists();
      
      // Write chunk to disk
      File outputFile(chunk_path, EFM_WRITE);
      outputFile.Write(encrypted->data(), encrypted->length());
      
      // ChunkMeta doesn't store path - it's derived from hash
      
      // Add to manifest
      chunk_manifest->_chunks.push_back(chunk_meta);
      
      // Update progress
      bytes_processed += chunk_data_size;
      total_compressed_size += chunk_meta._compressed_size;
      chunk_index++;
      
      // Report progress
      if (_progress_callback._item) {
        std::string progress_msg = FormatString("Processing chunk %zu/%zu for %s", 
          chunk_index, 
          _total_chunks, 
          input_file.c_str());
        float progress = float(bytes_processed) / float(fileSize);
        _progress_callback._item(progress_msg, progress);
      }
      
      // Check if cancelled
      if (_cancelled) {
        result.success = false;
        result.error_message = "Operation cancelled";
        return result;
      }
    }
    
    // Finalize the streaming hash to get the content hash
    file_hasher.finalize();
    Md5Sum content_md5_result = file_hasher.Result();
    std::string content_hash_string = content_md5_result.hex_digest();
    
    // Finalize storage hash (hash of all encrypted chunks)
    storage_hasher.finalize();
    Md5Sum storage_md5_result = storage_hasher.Result();
    std::string storage_hash_string = storage_md5_result.hex_digest();
    
    // Finalize XXHash to get the file hash for chunk manifest
    file_xxhasher->finish();
    chunk_manifest->_file_hash = file_xxhasher->result();
    
    result.success = true;
    result.compressed_size = total_compressed_size;
    result.compression_ratio = calculateCompressionRatio(result.original_size, result.compressed_size);
    result.content_hash = content_hash_string;  // MD5 of raw file content
    result.storage_hash = storage_hash_string;  // MD5 of all encrypted chunks
    result.processing_time = timer.SecsSinceStart();
    
  } catch (const std::exception& e) {
    result.success = false;
    result.error_message = FormatString("Exception processing chunked file: %s", e.what());
  }
  
  return result;
}

bool AssetPackagerImpl::processChunk(
    const datablock_ptr_t& chunk_data,
    size_t chunk_index,
    const file::Path& output_path,
    ChunkMeta& chunk_meta) {
  
  try {
    // Note: This method is currently not used since processChunkedFile
    // handles chunk processing inline. This is kept for potential
    // future refactoring where chunk processing might be parallelized.
    
    chunk_meta._size = chunk_data->length();
    
    // Compress chunk
    auto compressed = compressData(chunk_data);
    chunk_meta._compressed_size = compressed->length();
    
    // Encrypt chunk if enabled
    auto encrypted = encryptData(compressed);
    
    // Calculate chunk hash
    chunk_meta._hash = encrypted->hash();
    
    // Ensure output directory exists
    file::Path parent_dir = output_path;
    parent_dir.setFile("");
    parent_dir.ensureDirectoryExists();
    
    // Write chunk to disk
    File outputFile(output_path, EFM_WRITE);
    outputFile.Write(encrypted->data(), encrypted->length());
    
    // ChunkMeta doesn't store path - it's derived from hash
    
    return true;
    
  } catch (const std::exception& e) {
    logchan_catalog->log("ERROR: Failed to process chunk %zu: %s", chunk_index, e.what());
    return false;
  }
}

datablock_ptr_t AssetPackagerImpl::compressData(const datablock_ptr_t& _data) {
  if (!_data || _data->length() == 0) {
    return _data;
  }
  
  // Use compression level from config
  int level = _config->compression_level;
  
  switch (_config->compression_type) {
    case CompressionType::NONE:
      return _data;
      
    case CompressionType::LZ4:
    case CompressionType::LZ4HC:
      // For LZ4HC, ensure level is at least 1
      if (_config->compression_type == CompressionType::LZ4HC && level == 0) {
        level = 1;
      }
      return _data->compressed(level);
      
    default:
      // Unknown compression type, return uncompressed
      logchan_catalog->log("WARNING: Unknown compression type, returning uncompressed _data");
      return _data;
  }
}

datablock_ptr_t AssetPackagerImpl::encryptData(const datablock_ptr_t& _data) {
  if (!_codec || !_config->enable_encryption) {
    return _data;
  }
  
  // Encrypt the _data using the codec
  auto encrypted = std::make_shared<DataBlock>();
  
  // Call the codec's encrypt method
  auto encrypted_data = _codec->encrypt(_data.get());
  if (!encrypted_data) {
    logchan_catalog->log("ERROR: Encryption failed");
    return _data; // Return original _data on failure
  }
  
  // Return the encrypted data
  return encrypted_data;
}

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

bool shouldChunkFile(const file::Path& path, const PackageConfig& config) {
  try {
    File checkFile(path, EFM_READ);
    size_t fileSize = 0;
    checkFile.GetLength(fileSize);
    return fileSize > config.chunk_threshold;
  } catch (...) {
    return false;
  }
}

float calculateCompressionRatio(size_t original, size_t compressed) {
  if (original == 0) return 0.0f;
  return float(compressed) / float(original);
}

////////////////////////////////////////////////////////////////
// Legacy utility functions
////////////////////////////////////////////////////////////////

CompressionType getOptimalCompressionType(const file::Path& file_path) {
  // TODO: Determine optimal compression based on file type
  std::string ext = file_path.getExtension();
  
  // Images are often already compressed
  if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
    return CompressionType::NONE;
  }
  
  // Audio files are often already compressed
  if (ext == ".mp3" || ext == ".ogg" || ext == ".m4a") {
    return CompressionType::NONE;
  }
  
  // Use LZ4 for most other files
  return CompressionType::LZ4;
}

pattern_list_t getDefaultPackageFilters() {
  return {
    "*.png", "*.jpg", "*.jpeg", "*.tga", "*.bmp",  // Images
    "*.wav", "*.mp3", "*.ogg", "*.flac",           // Audio
    "*.obj", "*.fbx", "*.dae", "*.gltf",           // Models
    "*.txt", "*.json", "*.xml", "*.yaml",          // Data
    "*.glsl", "*.hlsl", "*.fx"                     // Shaders
  };
}

} // namespace ork::asset::catalog