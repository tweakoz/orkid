////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/chunk_manifest.h>
#include <ork/kernel/string/deco.inl>
#include <ork/file/file.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

namespace ork::asset::catalog {

////////////////////////////////////////////////////////////////
// ChunkManifest implementations moved from header
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// ChunkMeta
////////////////////////////////////////////////////////////////

// ChunkMeta methods implemented in header

////////////////////////////////////////////////////////////////
// ChunkManifest
////////////////////////////////////////////////////////////////

// ChunkManifest constructor/destructor - using default


bool ChunkManifest::isValid() const {
  if (_chunks.empty()) return false;
  if (_chunks.size() == 0) return false;
  if (_total_size == 0) return false;
  if (_file_hash == 0) return false;
  
  // Verify chunks are contiguous and sized correctly
  chunk_offset_t expected_offset = 0;
  for (const auto& chunk : _chunks) {
    if (chunk._offset != expected_offset) return false;
    expected_offset += chunk._size;
  }
  
  return expected_offset == _total_size;
}

// getChunk methods not in header

// getChunksForRange method not in header

// calculateTotalSize and verifyIntegrity methods not in header



// ChunkManifestBuilder not in headers - removing

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

const char* compressionTypeToString(CompressionType type) {
  switch (type) {
    case CompressionType::NONE: return "none";
    case CompressionType::LZ4: return "lz4";
    case CompressionType::LZ4HC: return "lz4hc";
    default: return "unknown";
  }
}

CompressionType compressionTypeFromString(const std::string& str) {
  if (str == "lz4") return CompressionType::LZ4;
  if (str == "lz4hc") return CompressionType::LZ4HC;
  return CompressionType::NONE;
}

////////////////////////////////////////////////////////////////////////////////
// ChunkManifest::toJson - Serialize to rapidjson::Value
////////////////////////////////////////////////////////////////////////////////

void ChunkManifest::toJson(void* value_ptr, void* allocator_ptr) const {
  auto& value = *static_cast<rapidjson::Value*>(value_ptr);
  auto& allocator = *static_cast<rapidjson::Document::AllocatorType*>(allocator_ptr);
  value.SetObject();
  
  // Add chunk manifest fields
  value.AddMember("chunk_size", static_cast<uint64_t>(chunk_size), allocator);
  value.AddMember("total_size", static_cast<uint64_t>(_total_size), allocator);
  value.AddMember("file_hash", static_cast<uint64_t>(_file_hash), allocator);
  value.AddMember("compression", rapidjson::Value(compressionTypeToString(_compression), allocator), allocator);
  value.AddMember("is_encrypted", _is_encrypted, allocator);
  
  // Add chunks array
  rapidjson::Value chunks_array(rapidjson::kArrayType);
  for (const auto& chunk : _chunks) {
    rapidjson::Value chunk_obj(rapidjson::kObjectType);
    chunk_obj.AddMember("offset", static_cast<uint64_t>(chunk._offset), allocator);
    chunk_obj.AddMember("size", static_cast<uint64_t>(chunk._size), allocator);
    chunk_obj.AddMember("compressed_size", static_cast<uint64_t>(chunk._compressed_size), allocator);
    chunk_obj.AddMember("hash", static_cast<uint64_t>(chunk._hash), allocator);
    chunks_array.PushBack(chunk_obj, allocator);
  }
  value.AddMember("chunks", chunks_array, allocator);
}

////////////////////////////////////////////////////////////////////////////////
// ChunkManifest::fromJson - Deserialize from rapidjson::Value
////////////////////////////////////////////////////////////////////////////////

void ChunkManifest::fromJson(const void* value_ptr) {
  const auto& value = *static_cast<const rapidjson::Value*>(value_ptr);
  if (!value.IsObject()) {
    throw std::runtime_error("ChunkManifest::fromJson: value is not an object");
  }
  
  // Read chunk manifest fields
  if (value.HasMember("chunk_size") && value["chunk_size"].IsUint64()) {
    // chunk_size is static constexpr, cannot be modified
  }
  
  if (value.HasMember("total_size") && value["total_size"].IsUint64()) {
    _total_size = value["total_size"].GetUint64();
  }
  
  if (value.HasMember("file_hash") && value["file_hash"].IsUint64()) {
    _file_hash = value["file_hash"].GetUint64();
  }
  
  if (value.HasMember("compression") && value["compression"].IsString()) {
    _compression = compressionTypeFromString(value["compression"].GetString());
  }
  
  if (value.HasMember("is_encrypted") && value["is_encrypted"].IsBool()) {
    _is_encrypted = value["is_encrypted"].GetBool();
  }
  
  // Read chunks array
  if (value.HasMember("chunks") && value["chunks"].IsArray()) {
    _chunks.clear();
    const auto& chunks_array = value["chunks"];
    
    for (rapidjson::SizeType i = 0; i < chunks_array.Size(); ++i) {
      const auto& chunk_obj = chunks_array[i];
      if (chunk_obj.IsObject()) {
        ChunkMeta chunk;
        
        if (chunk_obj.HasMember("offset") && chunk_obj["offset"].IsUint64()) {
          chunk._offset = chunk_obj["offset"].GetUint64();
        }
        
        if (chunk_obj.HasMember("size") && chunk_obj["size"].IsUint64()) {
          chunk._size = chunk_obj["size"].GetUint64();
        }
        
        if (chunk_obj.HasMember("compressed_size") && chunk_obj["compressed_size"].IsUint64()) {
          chunk._compressed_size = chunk_obj["compressed_size"].GetUint64();
        }
        
        if (chunk_obj.HasMember("hash") && chunk_obj["hash"].IsUint64()) {
          chunk._hash = chunk_obj["hash"].GetUint64();
        }
        
        _chunks.push_back(chunk);
      }
    }
  }
}

} // namespace ork::asset::catalog