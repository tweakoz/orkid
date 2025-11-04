////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/kernel/datablock.h>
#if !defined(ORK_IOS)
#include <ork/util/crypt.h>
#endif
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/chunk_manifest.h>
#include <ork/file/path.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/mutex.h>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>

namespace ork::asset::catalog {

// Type aliases moved to types.h

////////////////////////////////////////////////////////////////////////////////
// Result of chunk assembly operation
////////////////////////////////////////////////////////////////////////////////

struct ChunkAssemblyResult {
  bool success = false;
  datablock_ptr_t assembled_data;
  std::string error_message;
  
  // Performance metrics
  double assembly_time = 0.0;          // Total time to assemble
  size_t bytes_processed = 0;          // Total bytes processed
  int chunks_processed = 0;            // Number of chunks assembled
  
  // Validation results
  bool hash_valid = false;             // Overall file hash matches
  std::vector<int> invalid_chunks;     // List of chunks that failed validation
  
  // Get human-readable summary
  std::string getSummary() const;
};

////////////////////////////////////////////////////////////////////////////////
// Chunk assembler - reassembles files from chunks
// Handles decryption, decompression, and validation
//
// Key responsibilities:
// - Reassemble large assets from downloaded chunks
// - Decrypt chunks using provided codec (if encrypted)
// - Decompress chunks after decryption
// - Verify chunk hashes and overall file hash
// - Support both in-memory and streaming assembly for very large files
// - Progress tracking for long assembly operations
//
// Streaming mode automatically activates for files > max_memory_usage
// This allows processing of assets larger than available RAM
////////////////////////////////////////////////////////////////////////////////

struct ChunkAssembler {
  ////////////////////////////////////////////////////////////////////////////////
  // Configuration
  ////////////////////////////////////////////////////////////////////////////////
  struct Config {
    bool verify_chunk_hashes = true;    // Verify each chunk's hash
    bool verify_file_hash = true;       // Verify complete file hash
    bool parallel_processing = true;    // Process chunks in parallel
    size_t max_memory_usage = 512 * 1024 * 1024; // Max memory for assembly (512MB)
    file::Path temp_dir;               // Temporary directory for large files
  };
  
  ChunkAssembler(
    chunkmanifest_ptr_t chunk_manifest,
    const Config& config
  );
  
  // With codec for encrypted chunks
  ChunkAssembler(
    chunkmanifest_ptr_t chunk_manifest,
    encryptioncodec_ptr_t codec,
    const Config& config
  );
  
  ~ChunkAssembler();
  
  ////////////////////////////////////////////////////////////////////////////////
  // Assembly from memory chunks
  ////////////////////////////////////////////////////////////////////////////////
  
  // Assemble from vector of chunks in memory
  chunkassemblyresult_ptr_t assembleFromChunks(
    const datablock_list_t& chunks
  );
  
  // Assemble from map of chunks (chunk_index -> data)
  chunkassemblyresult_ptr_t assembleFromChunkMap(
    const chunk_data_map_t& chunk_map
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Assembly from files
  ////////////////////////////////////////////////////////////////////////////////
  
  // Assemble from chunk files on disk
  chunkassemblyresult_ptr_t assembleFromFiles(
    const chunk_file_list_t& chunk_files
  );
  
  // Assemble from directory containing chunks
  chunkassemblyresult_ptr_t assembleFromDirectory(
    const file::Path& chunk_dir,
    const std::string& base_filename
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Streaming assembly for large files
  ////////////////////////////////////////////////////////////////////////////////
  
  // Begin streaming assembly
  bool beginStreamingAssembly(const file::Path& output_file);
  
  // Add chunk to streaming assembly
  bool addChunkToStream(chunk_index_t chunk_index, datablock_ptr_t chunk_data);
  
  // Complete streaming assembly
  chunkassemblyresult_ptr_t completeStreamingAssembly();
  
  // Abort streaming assembly
  void abortStreamingAssembly();
  
  ////////////////////////////////////////////////////////////////////////////////
  // Progress tracking
  ////////////////////////////////////////////////////////////////////////////////
  
  
  // Get current progress
  float getProgress() const;
  
  
  ////////////////////////////////////////////////////////////////////////////////
  // Chunk operations
  ////////////////////////////////////////////////////////////////////////////////
  
  // Process a single chunk (decrypt/decompress)
  datablock_ptr_t processChunk(
    datablock_ptr_t chunk_data,
    chunk_index_t chunk_index
  );
  
  // Verify a chunk's hash
  bool verifyChunk(
    const datablock_ptr_t& chunk_data,
    chunk_index_t chunk_index
  );
  
  ////////////////////////////////////////////////////////////////////////////////
  // Utilities
  ////////////////////////////////////////////////////////////////////////////////
  
  // Calculate total size of assembled file
  size_t calculateAssembledSize() const;
  
  // Get chunk info
  
  // Check if we need streaming assembly based on size
  bool requiresStreamingAssembly() const;
  
  // Members
  chunkmanifest_ptr_t _chunk_manifest;
  encryptioncodec_ptr_t _codec;
  Config _config;
  
  // Progress tracking
  std::atomic<int> _chunks_processed{0};
  std::atomic<bool> _cancelled{false};
  
  // Implementation
  svar64_t _impl;
};

////////////////////////////////////////////////////////////////////////////////
// Chunk disassembler - splits files into chunks
// Used during packaging, inverse of ChunkAssembler
////////////////////////////////////////////////////////////////////////////////

struct ChunkDisassemblyResult {
  bool _success = false;
  datablock_list_t _chunks;
  chunkmanifest_ptr_t _chunk_manifest;
  double _processing_time = 0.0;
};

using chunkdisassemblyresult_ptr_t = std::shared_ptr<ChunkDisassemblyResult>;

struct ChunkDisassembler {
  ////////////////////////////////////////////////////////////////////////////////
  // Result of disassembly
  ////////////////////////////////////////////////////////////////////////////////
  
  ////////////////////////////////////////////////////////////////////////////////
  // Disassemble a file into chunks
  ////////////////////////////////////////////////////////////////////////////////
  static chunkdisassemblyresult_ptr_t disassemble(
    datablock_ptr_t data,
    encryptioncodec_ptr_t codec = nullptr,
    CompressionType compression = CompressionType::NONE
  );
};


} // namespace ork::asset::catalog