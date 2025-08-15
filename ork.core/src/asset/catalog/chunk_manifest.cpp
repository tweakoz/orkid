////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/chunk_manifest.h>
#include <ork/kernel/string/deco.inl>
#include <ork/file/file.h>

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

std::string ChunkManifest::getChunkFilename(const std::string& base_name, chunk_index_t chunk_index) const {
  // TODO: Implement chunk filename generation
  return base_name + ".chunk." + std::to_string(chunk_index);
}

chunkmanifest_ptr_t ChunkManifest::generateForFile(const file::Path& path, chunk_size_t chunk_size) {
  // TODO: Implement file generation
  return nullptr;
}

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

} // namespace ork::asset::catalog