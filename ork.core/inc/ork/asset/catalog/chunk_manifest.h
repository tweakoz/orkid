////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

// This file defines ChunkManifest (formerly ChunkedFileInfo)
// which describes how a file is split into chunks

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/kernel/svariant.h>
#include <ork/asset/catalog/types.h>
#include <vector>
#include <memory>

namespace ork::asset::catalog {

// Type aliases moved to types.h

////////////////////////////////////////////////////////////////////////////////
// Information about a single chunk of a large file
////////////////////////////////////////////////////////////////////////////////

struct ChunkMeta {
  chunk_offset_t _offset = 0;    // Offset of this chunk in the original file
  chunk_size_t _size = 0;        // Size of this chunk (uncompressed)
  chunk_size_t _compressed_size = 0;  // Size after compression (if applicable)
  chunk_hash_t _hash = 0;        // Hash of the chunk data (for verification)
  
};

////////////////////////////////////////////////////////////////////////////////
// Manifest describing how a file is split into chunks
// Used for efficient downloading and processing of large assets
// This is pure metadata - no runtime state
////////////////////////////////////////////////////////////////////////////////

struct ChunkManifest {
  static constexpr chunk_size_t chunk_size = 16<<20;      // 16MiB chunks
  static constexpr chunk_size_t chunk_threshold = 16<<20; // 10MB

  chunk_size_t _total_size = 0;                // Total size of original file
  chunk_hash_t _file_hash = 0;                 // Hash of complete file (used for filename)
  CompressionType _compression = CompressionType::NONE;
  bool _is_encrypted = false;
  
  chunk_meta_list_t _chunks;                   // Metadata for each chunk
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // Validation
  ////////////////////////////////////////////////////////////////////////////////
  bool isValid() const;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Serialization/Deserialization using rapidjson (forward declared)
  ////////////////////////////////////////////////////////////////////////////////
  void toJson(void* value, void* allocator) const;  // rapidjson::Value&, rapidjson::Document::AllocatorType&
  void fromJson(const void* value);                 // const rapidjson::Value&
  
};

// chunkmanifest_ptr_t defined in types.h

////////////////////////////////////////////////////////////////////////////////
// Helper to convert compression type to/from string
////////////////////////////////////////////////////////////////////////////////

const char* compressionTypeToString(CompressionType type);
CompressionType compressionTypeFromString(const std::string& str);

} // namespace ork::asset::catalog