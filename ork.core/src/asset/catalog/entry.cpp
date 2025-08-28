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
#include <ork/asset/catalog/packager.h>
#include <ork/asset/catalog/uploader.h>
#include <ork/asset/catalog/chunk_assembler.h>
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
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <ctime>

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
  doc.AddMember("compression", rapidjson::Value(compressionTypeToString(manifest->_compression), allocator), allocator);
  doc.AddMember("is_encrypted", manifest->_is_encrypted, allocator);
  
  // Add chunks array
  rapidjson::Value chunks_array(rapidjson::kArrayType);
  for (const auto& chunk : manifest->_chunks) {
    rapidjson::Value chunk_obj(rapidjson::kObjectType);
    chunk_obj.AddMember("offset", rapidjson::Value(static_cast<uint64_t>(chunk._offset)), allocator);
    chunk_obj.AddMember("size", rapidjson::Value(static_cast<uint64_t>(chunk._size)), allocator);
    chunk_obj.AddMember("compressed_size", rapidjson::Value(static_cast<uint64_t>(chunk._compressed_size)), allocator);
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

bool AssetEntry::isValid() const {
  // Basic validation
  if (_type.empty()) {
    return false;
  }
  
  // Must have hash
  if (_storage_hash.empty()) {
    return false;
  }
  
  // If chunked, must have chunk manifest
  if (_size > ChunkManifest::chunk_threshold && !_chunk_manifest) {
    // Large files should be chunked
    // This is a warning, not an error
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////

std::string AssetEntry::getValidationError() const {
  if (_type.empty()) {
    return "Asset type is empty";
  }
  
  if (_storage_hash.empty()) {
    return "Storage hash is required";
  }
  
  return "";
}

////////////////////////////////////////////////////////////////
// AssetEntry implementations moved from header
////////////////////////////////////////////////////////////////

bool AssetEntry::isChunked() const {
  return _chunk_manifest != nullptr;
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

std::string AssetEntry::toJson() const {
  logchan_catalog->log("CHUNK DEBUG: toJson for %s - _chunk_manifest=%p", 
                       _id.c_str(), _chunk_manifest.get());
  rapidjson::Document doc;
  doc.SetObject();
  auto& allocator = doc.GetAllocator();
  
  // Basic fields
  doc.AddMember("id", rapidjson::Value(_id.c_str(), allocator), allocator);
  doc.AddMember("namespace", rapidjson::Value(_namespace.c_str(), allocator), allocator);
  doc.AddMember("type", rapidjson::Value(_type.c_str(), allocator), allocator);
  doc.AddMember("priority", _priority, allocator);
  doc.AddMember("local", rapidjson::Value(_local_loc.c_str(), allocator), allocator);
  // filename field no longer used
  
  if (!_tar_root.empty()) {
    doc.AddMember("tar_root", rapidjson::Value(_tar_root.c_str(), allocator), allocator);
  }
  
  // Platforms
  rapidjson::Value platforms_array(rapidjson::kArrayType);
  for (const auto& platform : _platforms) {
    platforms_array.PushBack(rapidjson::Value(platform.c_str(), allocator), allocator);
  }
  doc.AddMember("platforms", platforms_array, allocator);
  
  // Dependencies - convert map back to list
  rapidjson::Value deps_array(rapidjson::kArrayType);
  for (const auto& [dep_id, dep_value] : _dependencies) {
    deps_array.PushBack(rapidjson::Value(dep_id.c_str(), allocator), allocator);
  }
  doc.AddMember("dependencies", deps_array, allocator);
  
  // Hash info
  doc.AddMember("content_hash", rapidjson::Value(_content_hash.c_str(), allocator), allocator);
  doc.AddMember("storage_hash", rapidjson::Value(_storage_hash.c_str(), allocator), allocator);
  doc.AddMember("hash_algorithm", rapidjson::Value(_hash_algorithm.c_str(), allocator), allocator);
  
  // Size info
  doc.AddMember("native_size", static_cast<uint64_t>(_size), allocator);
  if (_is_compressed) {
    doc.AddMember("compressed_size", static_cast<uint64_t>(_compressed_size), allocator);
  }
  
  // Chunk info if present - use ChunkManifest's own serialization
  if (_chunk_manifest) {
    logchan_catalog->log("CHUNK DEBUG: toJson for %s - serializing %zu chunks", 
                         _id.c_str(), _chunk_manifest->_chunks.size());
    rapidjson::Value chunks_obj(rapidjson::kObjectType);
    _chunk_manifest->toJson(&chunks_obj, &allocator);
    doc.AddMember("chunks", chunks_obj, allocator);
  }
  
  // Convert to string
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  doc.Accept(writer);
  
  return buffer.GetString();
}

////////////////////////////////////////////////////////////////////////////////

assetentry_ptr_t AssetEntry::fromJson(const std::string& json_str) {
  OrkAssert(false);
  rapidjson::Document doc;
  doc.Parse(json_str.c_str());
  
  if (doc.HasParseError()) {
    return nullptr;
  }
  
  auto entry = std::make_shared<AssetEntry>();
  
  // Basic fields
  if (doc.HasMember("id") && doc["id"].IsString()) {
    entry->_id = doc["id"].GetString();
  }
  
  if (doc.HasMember("namespace") && doc["namespace"].IsString()) {
    entry->_namespace = doc["namespace"].GetString();
  }
  
  if (doc.HasMember("type") && doc["type"].IsString()) {
    entry->_type = doc["type"].GetString();
  }
  
  if (doc.HasMember("priority") && doc["priority"].IsInt()) {
    entry->_priority = doc["priority"].GetInt();
  }
  
  
  if (doc.HasMember("local") && doc["local"].IsString()) {
    entry->_local_loc = doc["local"].GetString();
  }
  
  // filename field no longer used
  
  if (doc.HasMember("tar_root") && doc["tar_root"].IsString()) {
    entry->_tar_root = doc["tar_root"].GetString();
  }
  
  // Platforms
  if (doc.HasMember("platforms") && doc["platforms"].IsArray()) {
    const auto& platforms = doc["platforms"];
    for (rapidjson::SizeType i = 0; i < platforms.Size(); ++i) {
      if (platforms[i].IsString()) {
        entry->_platforms.push_back(platforms[i].GetString());
      }
    }
  }
  
  // Dependencies - expect as array
  if (doc.HasMember("dependencies") && doc["dependencies"].IsArray()) {
    const auto& deps = doc["dependencies"];
    for (rapidjson::SizeType i = 0; i < deps.Size(); ++i) {
      if (deps[i].IsString()) {
        std::string dep_id = deps[i].GetString();
        // Store in map format internally
        entry->_dependencies[dep_id] = dep_id;
      }
    }
  }
  
  // Hash info
  if (doc.HasMember("content_hash") && doc["content_hash"].IsString()) {
    entry->_content_hash = doc["content_hash"].GetString();
  }
  
  if (doc.HasMember("storage_hash") && doc["storage_hash"].IsString()) {
    entry->_storage_hash = doc["storage_hash"].GetString();
  }
  
  if (doc.HasMember("hash_algorithm") && doc["hash_algorithm"].IsString()) {
    entry->_hash_algorithm = doc["hash_algorithm"].GetString();
  }
  
  // Size info
  if (doc.HasMember("native_size") && doc["native_size"].IsUint64()) {
    entry->_size = doc["native_size"].GetUint64();
  }
  
  if (doc.HasMember("compressed_size") && doc["compressed_size"].IsUint64()) {
    entry->_compressed_size = doc["compressed_size"].GetUint64();
    entry->_is_compressed = true;
  }
  
  // Chunk info
  if (doc.HasMember("chunks") && doc["chunks"].IsObject()) {
    const auto& chunks_obj = doc["chunks"];
    
    // Create chunk manifest
    auto chunk_manifest = std::make_shared<ChunkManifest>();
        
    if (chunks_obj.HasMember("total_size") && chunks_obj["total_size"].IsUint64()) {
      chunk_manifest->_total_size = chunks_obj["total_size"].GetUint64();
    }
    
    if (chunks_obj.HasMember("file_hash") && chunks_obj["file_hash"].IsUint64()) {
      chunk_manifest->_file_hash = chunks_obj["file_hash"].GetUint64();
    }
    
    if (chunks_obj.HasMember("compression") && chunks_obj["compression"].IsString()) {
      chunk_manifest->_compression = compressionTypeFromString(chunks_obj["compression"].GetString());
    }
    
    if (chunks_obj.HasMember("is_encrypted") && chunks_obj["is_encrypted"].IsBool()) {
      chunk_manifest->_is_encrypted = chunks_obj["is_encrypted"].GetBool();
    }
    
    // Parse chunk list
    if (chunks_obj.HasMember("chunks") && chunks_obj["chunks"].IsArray()) {
      const auto& chunks_array = chunks_obj["chunks"];
      for (rapidjson::SizeType i = 0; i < chunks_array.Size(); ++i) {
        if (chunks_array[i].IsObject()) {
          const auto& chunk_obj = chunks_array[i];
          ChunkMeta chunk_meta;
          
          if (chunk_obj.HasMember("offset") && chunk_obj["offset"].IsUint64()) {
            chunk_meta._offset = chunk_obj["offset"].GetUint64();
          }
          
          if (chunk_obj.HasMember("size") && chunk_obj["size"].IsUint64()) {
            chunk_meta._size = chunk_obj["size"].GetUint64();
          }
          
          if (chunk_obj.HasMember("compressed_size") && chunk_obj["compressed_size"].IsUint64()) {
            chunk_meta._compressed_size = chunk_obj["compressed_size"].GetUint64();
          }
          
          if (chunk_obj.HasMember("hash") && chunk_obj["hash"].IsUint64()) {
            chunk_meta._hash = chunk_obj["hash"].GetUint64();
          }
          
          chunk_manifest->_chunks.push_back(chunk_meta);
        }
      }
    }
    
    entry->_chunk_manifest = chunk_manifest;
  }
  
  return entry;
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::repackage() - Repackage asset (recompute hashes, rechunk if needed)
////////////////////////////////////////////////////////////////////////////////
void AssetEntry::repackage() {
  
  // Check if we have valid file information
  if (_local_loc.empty()) {
    return;
  }
  
  // Resolve local path using getResolvedLocalPath()
  file::Path base_path = getResolvedLocalPath();
  if (base_path.empty()) {
    return;
  }
  
  // Only asset_pak supported
  if (_type != "asset_pak") {
    logchan_catalog->log("ERROR: Only asset_pak type is supported. Asset type: %s", _type.c_str());
    OrkAssert(false);
  }
  
  // Determine source directory using tar_root field
  file::Path source_dir;
  if (_tar_root.empty()) {
    // No tar_root specified - use base_path directly
    source_dir = base_path;
  } else {
    // Use tar_root to find the source directory
    source_dir = base_path / _tar_root;
  }
  
  // Check if directory exists
  if (!source_dir.doesPathExist()) {
    logchan_catalog->log("ERROR: Asset pak directory does not exist: %s", source_dir.c_str());
    OrkAssert(false);
  }
  
  // Create TAR from directory and store it for later use
  auto catalog = getCatalog();
  datablock_ptr_t tar_data;
  
  if (catalog) {
    // Pass 'this' directly to avoid lookup issues during creation
    auto self = std::make_shared<AssetEntry>(*this);
    auto pak_result = catalog->packFromLocal(self);
    if (pak_result && pak_result->isSuccess() && pak_result->_data) {
      // Store TAR _data for encryption later
      tar_data = pak_result->_data;
      
      // Update size from packed data
      _size = tar_data->length();
      
      // Compute content hash from TAR data
      CMD5 content_hasher;
      content_hasher.update(tar_data->data(), tar_data->length());
      content_hasher.finalize();
      Md5Sum content_md5_result = content_hasher.Result();
      _content_hash = content_md5_result.hex_digest();
      
      // Store the TAR _data in a member variable so we don't have to recreate it
      _temp_tar_data = tar_data;
    } else {
      logchan_catalog->log("ERROR: Failed to pack asset_pak from directory: %s", source_dir.c_str());
      OrkAssert(false);
    }
  } else {
    logchan_catalog->log("ERROR: No catalog available for asset_pak packing");
    OrkAssert(false);
  }
  
  // Package the file (encrypt)
  // Get codec from parent manifest
  encryptioncodec_ptr_t codec;
  auto parent_manifest = _parent_manifest.lock();
  if (parent_manifest) {
    codec = parent_manifest->getCodec();
    printf("[DEBUG REPACKAGE] Got codec from parent manifest for namespace: %s\n", _namespace.c_str());
  }
  
  if (codec) {
    printf("[DEBUG REPACKAGE] Using codec to encrypt asset: %s\n", _id.c_str());
    // Get the TAR data to encrypt
    std::vector<uint8_t> file_content;
    
    // For asset_pak, use the TAR data we already created
    if (_temp_tar_data) {
      file_content.resize(_temp_tar_data->length());
      memcpy(file_content.data(), _temp_tar_data->data(), _temp_tar_data->length());
      
      // Clear the temporary data after use
      _temp_tar_data.reset();
    } else {
      logchan_catalog->log("ERROR: No TAR data available for asset_pak encryption");
      return;
    }
    
    if (!file_content.empty()) {
      
      // Encrypt the content
      auto input_block = std::make_shared<DataBlock>(file_content.data(), file_content.size());
      auto encrypted_block = codec->encrypt(input_block.get());
      
      if (encrypted_block) {
        // Compute storage hash from encrypted data
        // Calculate MD5 storage hash of encrypted data
        CMD5 storage_hasher;
        storage_hasher.update(encrypted_block->data(), encrypted_block->length());
        storage_hasher.finalize();
        Md5Sum storage_md5_result = storage_hasher.Result();
        _storage_hash = storage_md5_result.hex_digest();
        
        // Get catalog from parent manifest to access cache directory
        file::Path enc_dir;
        auto catalog = parent_manifest->getParentCatalog();
        if (catalog) {
          enc_dir = catalog->getEncryptedDir();
        } else {
          // Fallback to default location
          enc_dir = file::Path::stage_dir() / "assetcache" / "enc";
        }
        // Ensure enc directory exists
        enc_dir.ensureDirectoryExists();
        file::Path encrypted_path = enc_dir / (_storage_hash + ".enc");
        
        std::ofstream out_file(encrypted_path.c_str(), std::ios::binary);
        if (out_file.is_open()) {
          out_file.write(reinterpret_cast<const char*>(encrypted_block->data()), encrypted_block->length());
          out_file.close();
          logchan_catalog->log("Wrote encrypted file: %s", encrypted_path.c_str());
        }
      }
    }
  } else {
    // No codec available - fallback to fake hash
    CMD5 storage_hasher;
    std::string fake_data = _content_hash + "_encrypted";
    storage_hasher.update((const unsigned char*)fake_data.c_str(), fake_data.length());
    storage_hasher.finalize();
    Md5Sum storage_md5_result = storage_hasher.Result();
    _storage_hash = storage_md5_result.hex_digest();
  }
  
  // Check if file needs chunking
  logchan_catalog->log("CHUNK DEBUG: Asset %s - size=%zu, threshold=%zu, codec=%p, type=%s", 
                       _id.c_str(), _size, ChunkManifest::chunk_threshold, codec.get(), _type.c_str());
  if (_size > ChunkManifest::chunk_threshold) {
    // Only chunk if we have encrypted data available
    if (codec && _type == "asset_pak") {
      logchan_catalog->log("Asset %s size %zu exceeds threshold, will chunk", _id.c_str(), _size);
      
      // Get encrypted file that was just saved
      file::Path enc_dir;
      auto catalog = parent_manifest->getParentCatalog();
      if (catalog) {
        enc_dir = catalog->getEncryptedDir();
      } else {
        enc_dir = file::Path::stage_dir() / "assetcache" / "enc";
      }
      file::Path encrypted_path = enc_dir / (_storage_hash + ".enc");
      
      // Read the encrypted data back
      if (encrypted_path.doesPathExist()) {
        File enc_file(encrypted_path, EFM_READ);
        size_t enc_size = 0;
        enc_file.GetLength(enc_size);
        
        auto encrypted_data = std::make_shared<DataBlock>();
        encrypted_data->reserve(enc_size);
        encrypted_data->_storage.resize(enc_size);
        enc_file.Read(const_cast<uint8_t*>(encrypted_data->data()), enc_size);
        
        // Use ChunkDisassembler to split into chunks
        auto disassembly_result = ChunkDisassembler::disassemble(
          encrypted_data,
          nullptr,  // Already encrypted, don't encrypt again
          CompressionType::NONE  // Already processed
        );
        
        if (disassembly_result.success && disassembly_result.chunk_manifest) {
          _chunk_manifest = disassembly_result.chunk_manifest;
          logchan_catalog->log("CHUNK DEBUG: Set _chunk_manifest for %s - %zu chunks, total_size=%zu", 
                              _id.c_str(), _chunk_manifest->_chunks.size(), _chunk_manifest->_total_size);
          
          // Get chunks directory
          file::Path chunks_dir;
          if (catalog) {
            chunks_dir = catalog->getChunksDir();
          } else {
            chunks_dir = file::Path::stage_dir() / "assetcache" / "enc" / "chunks";
          }
          chunks_dir.ensureDirectoryExists();
          
          // Save each chunk with proper hash-based naming
          for (size_t i = 0; i < disassembly_result.chunks.size(); ++i) {
            const auto& chunk_data = disassembly_result.chunks[i];
            const auto& chunk_meta = _chunk_manifest->_chunks[i];
            
            // Chunk filename: {chunk_hash}.chunk.{index:04d}
            std::string chunk_filename = FormatString("%llu.chunk.%04zu", 
                                                     chunk_meta._hash, i);
            file::Path chunk_path = chunks_dir / chunk_filename;
            
            // Write chunk to disk
            File chunk_file(chunk_path, EFM_WRITE);
            chunk_file.Write(chunk_data->data(), chunk_data->length());
            
            logchan_catalog->log("Saved chunk %zu/%zu: %s (size: %zu)", 
                                i + 1, disassembly_result.chunks.size(),
                                chunk_filename.c_str(), chunk_data->length());
          }
          
          // Save chunk manifest
          file::Path manifest_path = enc_dir / (_storage_hash + ".chunkmanifest");
          saveChunkManifest(_chunk_manifest, manifest_path);
          logchan_catalog->log("Saved chunk manifest: %s", manifest_path.c_str());
          
        } else {
          logchan_catalog->log("ERROR: Failed to disassemble into chunks: %s", 
                              disassembly_result.error_message.c_str());
          _chunk_manifest.reset();
          logchan_catalog->log("CHUNK DEBUG: Reset _chunk_manifest for %s (disassembly failed)", _id.c_str());
        }
      } else {
        logchan_catalog->log("ERROR: Encrypted file not found for chunking: %s", 
                            encrypted_path.c_str());
        _chunk_manifest.reset();
        logchan_catalog->log("CHUNK DEBUG: Reset _chunk_manifest for %s (encrypted file not found)", _id.c_str());
      }
    } else {
      // Non-pak or no codec - no chunking
      _chunk_manifest.reset();
      logchan_catalog->log("CHUNK DEBUG: Reset _chunk_manifest for %s (non-pak or no codec)", _id.c_str());
    }
  } else {
    // Small file - no chunking needed
    _chunk_manifest.reset();
    logchan_catalog->log("CHUNK DEBUG: Reset _chunk_manifest for %s (size %zu <= threshold)", _id.c_str(), _size);
  }
  
  logchan_catalog->log("CHUNK DEBUG: End of repackage for %s - _chunk_manifest=%p", 
                       _id.c_str(), _chunk_manifest.get());
  
  // Update compression info
  _is_compressed = false; // Will be true after actual compression
  _compressed_size = _size; // Will be updated after compression
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
    locationinfo_ptr_t location_info) const {
  
  // Starting asset upload
  
  // Verify entry is repackaged
  if (!isRepackaged()) {
    logchan_catalog->log("ERROR: Asset not repackaged");
    throw std::runtime_error("Asset must be repackaged before upload");
  }
  // Asset is repackaged
  
  // _chunk_manifest should already be set by repackage() if chunking was needed
  // Cannot load it here as upload() is a const function
  
  // Resolve destination from config
  // Resolving remote destination
  
  // Get API key from namespace encryption key if not set in location
  if (!location_info->_api_key_write.has_value()) {
    std::string encryption_key = config.getEncryptionKeyForNamespace(_namespace);
    if (!encryption_key.empty()) {
      location_info->_api_key_write = encryption_key;
      // Using namespace key as API key
    }
  }
  
  // Check if password authentication is required for uploads
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
  
  // Create upload receipt
  // Creating upload receipt
  auto receipt = std::make_shared<UploadReceipt>();
  receipt->upload_id = _id;  // Use asset id as upload id
  receipt->namespace_id = _namespace;
  receipt->destination = location_info->_upload_url.toString();
  receipt->timestamp = time(nullptr);
  // Upload receipt created
  
  // Get catalog for paths
  // Getting parent manifest and catalog
  auto manifest = getParentManifest();
  if (!manifest) {
    logchan_catalog->log("ERROR: Asset entry has no parent manifest");
    throw std::runtime_error("Asset entry has no parent manifest");
  }
  auto catalog = manifest->getParentCatalog();
  if (!catalog) {
    logchan_catalog->log("ERROR: Asset manifest has no parent catalog");
    throw std::runtime_error("Asset manifest has no parent catalog");
  }
  // Got catalog
  
  if (_chunk_manifest) {
    // CHUNKED: Upload manifest + chunks
    
    // 1. Upload chunk manifest
    auto manifest_path = catalog->getEncryptedDir() / (_storage_hash + ".chunkmanifest");
    if(0)logchan_catalog->log("Uploading chunk manifest: %s", manifest_path.c_str());
    
    // Check if manifest file exists
    if (!manifest_path.doesPathExist()) {
      logchan_catalog->log("ERROR: Chunk manifest file not found: %s", manifest_path.c_str());
      receipt->success = false;
      receipt->status_message = "Chunk manifest file not found: " + manifest_path.toStdString();
      return receipt;
    }
    
    auto manifest_upload = std::make_shared<Upload>();
    manifest_upload->_source_path = manifest_path;
    
    // Use catalog's URL generation
    manifest_upload->_destination_url = catalog->getChunkManifestUploadURL(this, location_info);
    
    manifest_upload->_api_key = location_info->_api_key_write;
    manifest_upload->_ignore_tls_errors = location_info->_disable_cert_check;
    
    if (!manifest_upload->execute()) {
      receipt->success = false;
      receipt->status_message = "Failed to upload chunk manifest: " + manifest_upload->_error_message;
      saveReceipt(receipt);
      return receipt;
    }
    
    // Add manifest to files list
    UploadFileEntry manifest_entry;
    manifest_entry.relative_path = manifest_path.getName();
    manifest_entry.remote_path = manifest_upload->_destination_url.toString();
    manifest_entry.size = manifest_upload->_total_bytes;
    manifest_entry.success = true;
    receipt->files.push_back(manifest_entry);
    receipt->bytes_uploaded += manifest_upload->_total_bytes;
    
    // Store chunk manifest in receipt for transparency
    receipt->_chunk_manifest = _chunk_manifest;
    
    // 2. Upload chunks using batch upload for concurrency
    logchan_catalog->log("Starting concurrent upload of %zu chunks", _chunk_manifest->_chunks.size());
    
    // Collect all chunk files and their remote paths
    std::vector<file::Path> chunk_files;
    std::vector<std::string> chunk_remote_paths;
    std::vector<URL> chunk_urls;
    std::vector<size_t> chunk_sizes;
    
    for (size_t chunk_idx = 0; chunk_idx < _chunk_manifest->_chunks.size(); ++chunk_idx) {
      const auto& chunk = _chunk_manifest->_chunks[chunk_idx];
      auto chunk_path = catalog->getChunksDir() / 
                       (std::to_string(chunk._hash) + ".chunk." + formatChunkIndex(chunk_idx));
      
      // Check if chunk file exists
      if (!chunk_path.doesPathExist()) {
        logchan_catalog->log("ERROR: Chunk file not found: %s", chunk_path.c_str());
        receipt->success = false;
        receipt->status_message = "Chunk file not found: " + chunk_path.toStdString();
        saveReceipt(receipt);
        return receipt;
      }
      
      chunk_files.push_back(chunk_path);
      
      // Get the URL and extract just the path part we need
      URL chunk_url = catalog->getChunkUploadURL(this, chunk_idx, chunk._hash, location_info);
      chunk_urls.push_back(chunk_url);
      
      // Extract relative path from URL for the uploader
      // The URL path should be something like /upload/namespace/enc/chunks/hash.chunk.0000
      // We need just the last part: hash.chunk.0000
      std::string url_path = chunk_url._path;
      size_t last_slash = url_path.rfind('/');
      std::string remote_path = (last_slash != std::string::npos) 
                                ? url_path.substr(last_slash + 1)
                                : url_path;
      chunk_remote_paths.push_back(remote_path);
      chunk_sizes.push_back(chunk._size);
    }
    
    // Create HTTPS uploader config from location info
    auto https_config = std::make_shared<HttpsUploaderConfig>();
    
    // Parse the first chunk URL to get host/port settings
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
      https_config->remote_base_path = (last_slash != std::string::npos) 
                                       ? url_path.substr(0, last_slash)
                                       : "/";
    }
    
    // Create HTTPS uploader and perform batch upload
    HttpsUploader uploader(https_config);
    bool chunks_success = uploader.uploadFiles(chunk_files, chunk_remote_paths);
    
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
    
    // All chunks uploaded successfully
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
    
  } else {
    // REGULAR: Upload single encrypted file
    // Regular file upload
    auto enc_path = getLocalEncryptedPath();
    // Encrypted file path set
    
    // Check if file exists
    if (!enc_path.doesPathExist()) {
      logchan_catalog->log("ERROR: Encrypted file does not exist: %s", enc_path.c_str());
      receipt->success = false;
      receipt->status_message = "Encrypted file not found: " + enc_path.toStdString();
      return receipt;
    }
    // Encrypted file exists
    
    // Create upload object
    // Creating Upload object
    auto file_upload = std::make_shared<Upload>();
    file_upload->_source_path = enc_path;
    
    // Use catalog's URL generation
    file_upload->_destination_url = catalog->getAssetUploadURL(this, location_info);
    
    // Upload configuration set
    
    file_upload->_api_key = location_info->_api_key_write;
    file_upload->_ignore_tls_errors = location_info->_disable_cert_check;
    
    // API key and TLS settings configured
    
    // Executing upload
    if (!file_upload->execute()) {
      logchan_catalog->log("ERROR: Upload failed! Error: %s (state: %d)", 
                           file_upload->_error_message.c_str(), (int)file_upload->_state.load());
      
      UploadFileEntry file_entry;
      file_entry.relative_path = enc_path.getName();
      file_entry.remote_path = file_upload->_destination_url.toString();
      file_entry.success = false;
      file_entry.error_message = file_upload->_error_message;
      receipt->files.push_back(file_entry);
      receipt->failed_files++;
      
      receipt->success = false;
      receipt->status_message = "Failed to upload file: " + file_upload->_error_message;
      saveReceipt(receipt);
      // Upload failed
      return receipt;
    }
    
    if(0)logchan_catalog->log("Upload succeeded! Bytes uploaded: %zu", file_upload->_bytes_uploaded.load());
    
    // Add successful file to receipt
    UploadFileEntry file_entry;
    file_entry.relative_path = enc_path.getName();
    file_entry.remote_path = file_upload->_destination_url.toString();
    file_entry.size = file_upload->_total_bytes;
    file_entry.hash = _storage_hash;
    file_entry.success = true;
    receipt->files.push_back(file_entry);
    receipt->bytes_uploaded = file_upload->_total_bytes;
    receipt->successful_files = 1;
    // Added successful file to receipt
  }
  
  // Set final receipt status
  // Setting final receipt status
  receipt->total_files = receipt->files.size();
  receipt->success = true;
  receipt->status_message = "Upload completed successfully";
  logchan_catalog->log("Upload receipt summary: %zu/%zu files successful", receipt->successful_files, receipt->total_files);
  
  // Saving receipt to disk
  saveReceipt(receipt);
  // Receipt saved
  
  // Upload completed successfully
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
