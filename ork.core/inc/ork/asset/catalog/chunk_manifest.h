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
  chunk_offset_t offset = 0;    // Offset of this chunk in the original file
  chunk_size_t size = 0;        // Size of this chunk (uncompressed)
  chunk_size_t compressed_size = 0;  // Size after compression (if applicable)
  chunk_hash_t hash = 0;        // Hash of the chunk data (for verification)
  
};

////////////////////////////////////////////////////////////////////////////////
// Manifest describing how a file is split into chunks
// Used for efficient downloading and processing of large assets
// This is pure metadata - no runtime state
////////////////////////////////////////////////////////////////////////////////

struct ChunkManifest {
  static constexpr chunk_size_t chunk_size = 4 * 1024 * 1024;                    // Default 4MB chunks
  static constexpr chunk_size_t chunk_threshold = 10 * 1024 * 1024; // 10MB

  chunk_size_t total_size = 0;                // Total size of original file
  chunk_hash_t file_hash = 0;                 // Hash of complete file (used for filename)
  CompressionType compression = CompressionType::NONE;
  bool is_encrypted = false;
  
  chunk_meta_list_t chunks;                   // Metadata for each chunk
  
  ////////////////////////////////////////////////////////////////////////////////
  // Calculate optimal chunk size based on file size
  // - Small files: single chunk
  // - Medium files: 4MB chunks
  // - Large files: up to 16MB chunks
  ////////////////////////////////////////////////////////////////////////////////
  static chunk_size_t calculateOptimalChunkSize(chunk_size_t file_size);
  
  ////////////////////////////////////////////////////////////////////////////////
  // Generate chunk metadata for a file
  // - Calculates chunk boundaries
  // - Does NOT compute hashes (done during packaging)
  ////////////////////////////////////////////////////////////////////////////////
  static chunkmanifest_ptr_t generateForFile(
    const file::Path& path, 
    chunk_size_t chunk_size = 0  // 0 = auto-calculate
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Validation
  ////////////////////////////////////////////////////////////////////////////////
  bool isValid() const;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Get chunk filename for a given index
  // Format: {file_hash}.chunk.{index}
  // Where file_hash is the hash of the complete file
  ////////////////////////////////////////////////////////////////////////////////
  std::string getChunkFilename(const std::string& base_name, chunk_index_t chunk_index) const;
  
};

// chunkmanifest_ptr_t defined in types.h

////////////////////////////////////////////////////////////////////////////////
// Helper to convert compression type to/from string
////////////////////////////////////////////////////////////////////////////////

const char* compressionTypeToString(CompressionType type);
CompressionType compressionTypeFromString(const std::string& str);

} // namespace ork::asset::catalog