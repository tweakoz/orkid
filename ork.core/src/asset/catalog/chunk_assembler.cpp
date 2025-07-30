////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/xxhash.inl>
#include <ork/util/logger.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// ChunkAssemblerImpl - Pimpl implementation
////////////////////////////////////////////////////////////////

struct ChunkAssemblerImpl {
  ChunkAssembler* _assembler;
  
  // Streaming assembly state
  struct StreamingState {
    file::Path output_path;
    int output_fd = -1; // POSIX file descriptor
    LockedResource<chunk_data_map_t> pending_chunks;
    std::atomic<chunk_index_t> next_chunk_index{0};
    std::atomic<size_t> bytes_written{0};
    Timer timer;
  };
  std::unique_ptr<StreamingState> _streaming_state;
  
  // Assembly statistics
  mutable LockedResource<ChunkAssemblyResult> _current_result;
  
  ChunkAssemblerImpl(ChunkAssembler* assembler) : _assembler(assembler) {}
  
  // Internal methods
  datablock_list_t processChunksParallel(const datablock_list_t& input_chunks);
  datablock_ptr_t assembleProcessedChunks(const datablock_list_t& processed_chunks);
  bool verifyFileHash(const datablock_ptr_t& assembled_data);
  bool writeChunkToStream(chunk_index_t chunk_index, const datablock_ptr_t& data);
  bool flushStreamBuffer();
};


////////////////////////////////////////////////////////////////
// ChunkAssemblyResult
////////////////////////////////////////////////////////////////

std::string ChunkAssemblyResult::getSummary() const {
  if (success) {
    return FormatString("Assembly successful: %d chunks, %.2f MB in %.2fs (%.1f MB/s)",
                       chunks_processed,
                       bytes_processed / (1024.0 * 1024.0),
                       assembly_time,
                       (bytes_processed / (1024.0 * 1024.0)) / assembly_time);
  } else {
    return FormatString("Assembly failed: %s", error_message.c_str());
  }
}

////////////////////////////////////////////////////////////////
// ChunkAssembler
////////////////////////////////////////////////////////////////

ChunkAssembler::ChunkAssembler(
    chunkmanifest_ptr_t chunk_manifest,
    const Config& config)
    : _chunk_manifest(chunk_manifest)
    , _config(config)
    , _chunks_processed(0)
    , _cancelled(false) {
  auto impl = _impl.makeShared<ChunkAssemblerImpl>(this);
}

ChunkAssembler::ChunkAssembler(
    chunkmanifest_ptr_t chunk_manifest,
    encryptioncodec_ptr_t codec,
    const Config& config)
    : _chunk_manifest(chunk_manifest)
    , _codec(codec)
    , _config(config)
    , _chunks_processed(0)
    , _cancelled(false) {
  auto impl = _impl.makeShared<ChunkAssemblerImpl>(this);
}

ChunkAssembler::~ChunkAssembler() {
  auto impl = _impl.getShared<ChunkAssemblerImpl>();
  if (impl && impl->_streaming_state) {
    abortStreamingAssembly();
  }
}

////////////////////////////////////////////////////////////////
// Assembly from memory chunks
////////////////////////////////////////////////////////////////

chunkassemblyresult_ptr_t ChunkAssembler::assembleFromChunks(
    const datablock_list_t& chunks) {
  auto result = std::make_shared<ChunkAssemblyResult>();
  auto impl = _impl.getShared<ChunkAssemblerImpl>();
  Timer timer;
  timer.Start();
  
  try {
    // Validate input
    if (!_chunk_manifest) {
      result->success = false;
      result->error_message = "No chunk manifest provided";
      return result;
    }
    
    if (chunks.size() != _chunk_manifest->chunks.size()) {
      result->success = false;
      result->error_message = FormatString("Chunk count mismatch: expected %zu, got %zu",
                                          _chunk_manifest->chunks.size(), chunks.size());
      return result;
    }
    
    // Process chunks in parallel (decrypt/decompress)
    auto processed_chunks = impl->processChunksParallel(chunks);
    
    // Check for cancellation
    if (_cancelled) {
      result->success = false;
      result->error_message = "Assembly cancelled";
      return result;
    }
    
    // Assemble chunks into final data
    auto assembled_data = impl->assembleProcessedChunks(processed_chunks);
    if (!assembled_data) {
      result->success = false;
      result->error_message = "Failed to assemble chunks";
      return result;
    }
    
    // Verify file hash
    if (!impl->verifyFileHash(assembled_data)) {
      result->success = false;
      result->error_message = "File hash verification failed";
      return result;
    }
    
    // Update result
    result->success = true;
    result->assembled_data = assembled_data;
    result->chunks_processed = chunks.size();
    result->bytes_processed = assembled_data->length();
    result->assembly_time = timer.SecsSinceStart();
    
    // Assembly complete
    
  } catch (const std::exception& e) {
    result->success = false;
    result->error_message = FormatString("Exception during assembly: %s", e.what());
  }
  
  return result;
}

chunkassemblyresult_ptr_t ChunkAssembler::assembleFromChunkMap(
    const std::map<size_t, datablock_ptr_t>& chunk_map) {
  auto result = std::make_shared<ChunkAssemblyResult>();
  
  // Validate we have all chunks
  if (!_chunk_manifest) {
    result->success = false;
    result->error_message = "No chunk manifest provided";
    return result;
  }
  
  // Convert map to ordered list
  datablock_list_t ordered_chunks;
  ordered_chunks.reserve(_chunk_manifest->chunks.size());
  
  for (size_t i = 0; i < _chunk_manifest->chunks.size(); ++i) {
    auto it = chunk_map.find(i);
    if (it == chunk_map.end()) {
      result->success = false;
      result->error_message = FormatString("Missing chunk %zu", i);
      return result;
    }
    ordered_chunks.push_back(it->second);
  }
  
  // Delegate to assembleFromChunks
  return assembleFromChunks(ordered_chunks);
}

////////////////////////////////////////////////////////////////
// Assembly from files
////////////////////////////////////////////////////////////////

chunkassemblyresult_ptr_t ChunkAssembler::assembleFromFiles(
    const std::vector<file::Path>& chunk_files) {
  auto result = std::make_shared<ChunkAssemblyResult>();
  
  try {
    // Validate input
    if (!_chunk_manifest) {
      result->success = false;
      result->error_message = "No chunk manifest provided";
      return result;
    }
    
    if (chunk_files.size() != _chunk_manifest->chunks.size()) {
      result->success = false;
      result->error_message = FormatString("File count mismatch: expected %zu, got %zu",
                                          _chunk_manifest->chunks.size(), chunk_files.size());
      return result;
    }
    
    // Read all chunks from files
    datablock_list_t chunks;
    chunks.reserve(chunk_files.size());
    
    for (size_t i = 0; i < chunk_files.size(); ++i) {
      const auto& chunk_file = chunk_files[i];
      
      // Check if file exists
      if (!FileEnv::GetRef().DoesFileExist(chunk_file)) {
        result->success = false;
        result->error_message = FormatString("Chunk file not found: %s", chunk_file.c_str());
        return result;
      }
      
      // Read chunk file
      File file(chunk_file, EFM_READ);
      size_t fileSize = 0;
      file.GetLength(fileSize);
      
      auto chunk_data = std::make_shared<DataBlock>();
      chunk_data->reserve(fileSize);
      chunk_data->_storage.resize(fileSize);
      file.Read(const_cast<uint8_t*>(chunk_data->data()), fileSize);
      
      chunks.push_back(chunk_data);
      
      // Progress tracked via _chunks_processed
    }
    
    // Delegate to assembleFromChunks
    return assembleFromChunks(chunks);
    
  } catch (const std::exception& e) {
    result->success = false;
    result->error_message = FormatString("Exception reading chunk files: %s", e.what());
    return result;
  }
}

chunkassemblyresult_ptr_t ChunkAssembler::assembleFromDirectory(
    const file::Path& chunk_dir,
    const std::string& base_filename) {
  auto result = std::make_shared<ChunkAssemblyResult>();
  
  try {
    // Validate input
    if (!_chunk_manifest) {
      result->success = false;
      result->error_message = "No chunk manifest provided";
      return result;
    }
    
    // Check if directory exists
    if (!FileEnv::GetRef().DoesDirectoryExist(chunk_dir)) {
      result->success = false;
      result->error_message = FormatString("Chunk directory not found: %s", chunk_dir.c_str());
      return result;
    }
    
    // Build list of chunk files based on manifest
    std::vector<file::Path> chunk_files;
    chunk_files.reserve(_chunk_manifest->chunks.size());
    
    for (size_t i = 0; i < _chunk_manifest->chunks.size(); ++i) {
      // Determine chunk filename
      // Format: chunk_{index:04d}_{hash:016x}.enc (if encrypted) or without .enc
      std::string chunk_filename = FormatString("chunk_%04zu_%016lx%s",
                                               i,
                                               _chunk_manifest->chunks[i].hash,
                                               _chunk_manifest->is_encrypted ? ".enc" : "");
      
      file::Path chunk_path = chunk_dir / base_filename / chunk_filename;
      chunk_files.push_back(chunk_path);
    }
    
    // Delegate to assembleFromFiles
    return assembleFromFiles(chunk_files);
    
  } catch (const std::exception& e) {
    result->success = false;
    result->error_message = FormatString("Exception assembling from directory: %s", e.what());
    return result;
  }
}

////////////////////////////////////////////////////////////////
// Streaming assembly
////////////////////////////////////////////////////////////////

bool ChunkAssembler::beginStreamingAssembly(const file::Path& output_file) {
  auto impl = _impl.getShared<ChunkAssemblerImpl>();
  if (impl->_streaming_state) {
    return false; // Already in streaming mode
  }
  
  if (!_chunk_manifest) {
    return false; // Need manifest to know expected chunks
  }
  
  try {
    impl->_streaming_state = std::make_unique<ChunkAssemblerImpl::StreamingState>();
    impl->_streaming_state->output_path = output_file;
    impl->_streaming_state->timer.Start();
    
    // Create output file using POSIX
    // Note: We open in write mode which will truncate any existing file
    // Ensure parent directory exists
    file::Path parent_dir = output_file;
    parent_dir.setFile("");  // Remove filename to get parent directory
    parent_dir.ensureDirectoryExists();
    
    // Open file with O_CREAT | O_WRONLY | O_TRUNC
    impl->_streaming_state->output_fd = ::open(output_file.c_str(), 
                                               O_CREAT | O_WRONLY | O_TRUNC,
                                               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    
    if (impl->_streaming_state->output_fd < 0) {
      logchan_catalog->log("ERROR: Failed to open output file: %s", output_file.c_str());
      impl->_streaming_state.reset();
      return false;
    }
    
    // Initialize the current result for tracking
    auto result = std::make_shared<ChunkAssemblyResult>();
    impl->_current_result.atomicOp([&result](ChunkAssemblyResult& current) {
      current = *result;
    });
    
    // Reset progress tracking
    _chunks_processed = 0;
    
    return true;
    
  } catch (const std::exception& e) {
    logchan_catalog->log("ERROR: Failed to begin streaming assembly: %s", e.what());
    impl->_streaming_state.reset();
    return false;
  }
}

bool ChunkAssembler::addChunkToStream(chunk_index_t chunk_index, datablock_ptr_t chunk_data) {
  auto impl = _impl.getShared<ChunkAssemblerImpl>();
  if (!impl->_streaming_state) {
    return false;
  }
  
  try {
    // Verify chunk
    if (!verifyChunk(chunk_data, chunk_index)) {
      return false;
    }
    
    // Process chunk (decrypt/decompress)
    auto processed = processChunk(chunk_data, chunk_index);
    if (!processed) {
      return false;
    }
    
    // Add to pending chunks
    impl->_streaming_state->pending_chunks.atomicOp([&](chunk_data_map_t& pending) {
      pending[chunk_index] = processed;
    });
    
    // Try to write sequential chunks
    bool wrote_chunks = false;
    impl->_streaming_state->pending_chunks.atomicOp([this, impl, &wrote_chunks](chunk_data_map_t& pending) {
      while (true) {
        auto next_index = impl->_streaming_state->next_chunk_index.load();
        auto it = pending.find(next_index);
        if (it == pending.end()) {
          break; // Next chunk not available yet
        }
        
        // Write chunk to file
        if (!impl->writeChunkToStream(next_index, it->second)) {
          break; // Write failed
        }
        
        // Update state
        impl->_streaming_state->bytes_written += it->second->length();
        impl->_streaming_state->next_chunk_index.fetch_add(1);
        _chunks_processed.fetch_add(1);
        pending.erase(it);
        wrote_chunks = true;
      }
    });
    
    return true;
    
  } catch (const std::exception& e) {
    logchan_catalog->log("ERROR: Failed to add chunk to stream: %s", e.what());
    return false;
  }
}

chunkassemblyresult_ptr_t ChunkAssembler::completeStreamingAssembly() {
  auto result = std::make_shared<ChunkAssemblyResult>();
  auto impl = _impl.getShared<ChunkAssemblerImpl>();
  
  if (!impl->_streaming_state) {
    result->success = false;
    result->error_message = "No streaming assembly in progress";
    return result;
  }
  
  try {
    // Check if we have all chunks
    size_t expected_chunks = _chunk_manifest ? _chunk_manifest->chunks.size() : 0;
    size_t processed_chunks = _chunks_processed.load();
    
    if (processed_chunks != expected_chunks) {
      result->success = false;
      result->error_message = FormatString("Incomplete assembly: processed %zu of %zu chunks",
                                          processed_chunks, expected_chunks);
      impl->_streaming_state.reset();
      return result;
    }
    
    // Check for any pending chunks
    size_t pending_count = 0;
    impl->_streaming_state->pending_chunks.atomicOp([&](const chunk_data_map_t& pending) {
      pending_count = pending.size();
    });
    
    if (pending_count > 0) {
      result->success = false;
      result->error_message = FormatString("%zu chunks still pending", pending_count);
      impl->_streaming_state.reset();
      return result;
    }
    
    // Close the output file first
    if (impl->_streaming_state->output_fd >= 0) {
      ::close(impl->_streaming_state->output_fd);
      impl->_streaming_state->output_fd = -1;
    }
    
    // Verify file size using stat
    struct stat st;
    if (::stat(impl->_streaming_state->output_path.c_str(), &st) != 0) {
      result->success = false;
      result->error_message = FormatString("Failed to stat output file: %s", strerror(errno));
      impl->_streaming_state.reset();
      return result;
    }
    
    size_t fileSize = st.st_size;
    
    if (fileSize != _chunk_manifest->total_size) {
      result->success = false;
      result->error_message = FormatString("File size mismatch: expected %zu, got %zu",
                                          _chunk_manifest->total_size, fileSize);
      impl->_streaming_state.reset();
      return result;
    }
    
    // Success
    result->success = true;
    result->chunks_processed = processed_chunks;
    result->bytes_processed = impl->_streaming_state->bytes_written.load();
    result->assembly_time = impl->_streaming_state->timer.SecsSinceStart();
    // Output path is in streaming_state->output_path
    
    // Streaming assembly complete
    
  } catch (const std::exception& e) {
    result->success = false;
    result->error_message = FormatString("Exception completing streaming assembly: %s", e.what());
  }
  
  impl->_streaming_state.reset();
  return result;
}

void ChunkAssembler::abortStreamingAssembly() {
  auto impl = _impl.getShared<ChunkAssemblerImpl>();
  if (impl->_streaming_state) {
    // Close the file if it's open
    if (impl->_streaming_state->output_fd >= 0) {
      ::close(impl->_streaming_state->output_fd);
      impl->_streaming_state->output_fd = -1;
    }
    
    // Try to delete partial file using unlink
    if (::access(impl->_streaming_state->output_path.c_str(), F_OK) == 0) {
      if (::unlink(impl->_streaming_state->output_path.c_str()) != 0) {
        logchan_catalog->log("WARNING: Failed to delete partial file at: %s (%s)", 
                             impl->_streaming_state->output_path.c_str(), strerror(errno));
      }
    }
    
    impl->_streaming_state.reset();
  }
}

////////////////////////////////////////////////////////////////
// Progress tracking
////////////////////////////////////////////////////////////////

float ChunkAssembler::getProgress() const {
  if (!_chunk_manifest || _chunk_manifest->chunks.empty()) {
    return 0.0f;
  }
  return (float)_chunks_processed.load() / (float)_chunk_manifest->chunks.size();
}

////////////////////////////////////////////////////////////////
// Chunk operations
////////////////////////////////////////////////////////////////

datablock_ptr_t ChunkAssembler::processChunk(
    datablock_ptr_t chunk_data,
    chunk_index_t chunk_index) {
  if (!chunk_data || chunk_data->length() == 0) {
    return chunk_data;
  }
  
  // Get chunk metadata
  if (!_chunk_manifest || chunk_index >= _chunk_manifest->chunks.size()) {
    return nullptr;
  }
  
  const auto& chunk_meta = _chunk_manifest->chunks[chunk_index];
  
  // Decrypt if needed
  datablock_ptr_t decrypted = chunk_data;
  if (_chunk_manifest->is_encrypted && _codec) {
    decrypted = _codec->decrypt(chunk_data.get());
    if (!decrypted) {
      logchan_catalog->log("ERROR: Failed to decrypt chunk %zu", chunk_index);
      return nullptr;
    }
  }
  
  // Decompress if needed
  datablock_ptr_t decompressed = decrypted;
  if (_chunk_manifest->compression != CompressionType::NONE) {
    decompressed = decrypted->decompressed();
    if (!decompressed) {
      logchan_catalog->log("ERROR: Failed to decompress chunk %zu", chunk_index);
      return nullptr;
    }
  }
  
  // Verify size matches expected
  if (decompressed->length() != chunk_meta.size) {
    logchan_catalog->log("ERROR: Chunk %zu size mismatch: expected %zu, got %zu",
                         chunk_index, chunk_meta.size, decompressed->length());
    return nullptr;
  }
  
  return decompressed;
}

bool ChunkAssembler::verifyChunk(
    const datablock_ptr_t& chunk_data,
    chunk_index_t chunk_index) {
  if (!_chunk_manifest || chunk_index >= _chunk_manifest->chunks.size()) {
    return false;
  }
  
  const auto& chunk_meta = _chunk_manifest->chunks[chunk_index];
  
  // Calculate hash of chunk data
  chunk_hash_t calculated_hash = chunk_data->hash();
  
  // Compare with expected hash
  if (calculated_hash != chunk_meta.hash) {
    logchan_catalog->log("ERROR: Chunk %zu hash mismatch: expected %016llx, got %016llx",
                         chunk_index, (unsigned long long)chunk_meta.hash, (unsigned long long)calculated_hash);
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////
// Utilities
////////////////////////////////////////////////////////////////

size_t ChunkAssembler::calculateAssembledSize() const {
  return _chunk_manifest ? _chunk_manifest->total_size : 0;
}

bool ChunkAssembler::requiresStreamingAssembly() const {
  return calculateAssembledSize() > _config.max_memory_usage;
}

////////////////////////////////////////////////////////////////
// ChunkAssemblerImpl Internal methods
////////////////////////////////////////////////////////////////

datablock_list_t ChunkAssemblerImpl::processChunksParallel(const datablock_list_t& input_chunks) {
  datablock_list_t processed_chunks;
  processed_chunks.resize(input_chunks.size());
  
  // Process chunks (could be parallelized with work_queue in future)
  for (size_t i = 0; i < input_chunks.size(); ++i) {
    // Verify chunk hash first
    if (!_assembler->verifyChunk(input_chunks[i], i)) {
      logchan_catalog->log("ERROR: Chunk %zu verification failed", i);
      return datablock_list_t(); // Return empty on failure
    }
    
    // Process chunk (decrypt/decompress)
    auto processed = _assembler->processChunk(input_chunks[i], i);
    if (!processed) {
      logchan_catalog->log("ERROR: Failed to process chunk %zu", i);
      return datablock_list_t(); // Return empty on failure
    }
    
    processed_chunks[i] = processed;
    
    // Update progress
    _assembler->_chunks_processed.fetch_add(1);
    
    // Check for cancellation
    if (_assembler->_cancelled) {
      return datablock_list_t(); // Return empty on cancellation
    }
  }
  
  return processed_chunks;
}

datablock_ptr_t ChunkAssemblerImpl::assembleProcessedChunks(const datablock_list_t& processed_chunks) {
  if (processed_chunks.empty()) {
    return nullptr;
  }
  
  // Calculate total size
  size_t total_size = 0;
  for (const auto& chunk : processed_chunks) {
    if (!chunk) {
      return nullptr; // Invalid chunk
    }
    total_size += chunk->length();
  }
  
  // Allocate final buffer
  auto assembled = std::make_shared<DataBlock>();
  assembled->reserve(total_size);
  
  // Copy all chunks into final buffer
  for (const auto& chunk : processed_chunks) {
    assembled->addData(chunk->data(), chunk->length());
  }
  
  return assembled;
}

bool ChunkAssemblerImpl::verifyFileHash(const datablock_ptr_t& assembled_data) {
  if (!assembled_data || !_assembler->_chunk_manifest) {
    return false;
  }
  
  // Calculate XXHash64 of assembled data (same as packager)
  auto xxhasher = std::make_shared<XXH64HASH>();
  xxhasher->init();
  xxhasher->accumulate(assembled_data->data(), assembled_data->length());
  xxhasher->finish();
  chunk_hash_t calculated_hash = xxhasher->result();
  
  // Compare with expected file hash
  if (calculated_hash != _assembler->_chunk_manifest->file_hash) {
    logchan_catalog->log("ERROR: File hash mismatch: expected %016llx, got %016llx",
                         (unsigned long long)_assembler->_chunk_manifest->file_hash, (unsigned long long)calculated_hash);
    return false;
  }
  
  return true;
}

bool ChunkAssemblerImpl::writeChunkToStream(chunk_index_t chunk_index, const datablock_ptr_t& data) {
  if (!_streaming_state || _streaming_state->output_fd < 0 || !data) {
    return false;
  }
  
  // Write chunk data using POSIX write
  const uint8_t* buffer = data->data();
  size_t bytes_to_write = data->length();
  size_t bytes_written = 0;
  
  while (bytes_written < bytes_to_write) {
    ssize_t result = ::write(_streaming_state->output_fd, 
                            buffer + bytes_written, 
                            bytes_to_write - bytes_written);
    
    if (result < 0) {
      if (errno == EINTR) {
        continue; // Interrupted, retry
      }
      logchan_catalog->log("ERROR: Failed to write chunk %zu: %s", chunk_index, strerror(errno));
      return false;
    }
    
    bytes_written += result;
  }
  
  return true;
}

bool ChunkAssemblerImpl::flushStreamBuffer() {
  // TODO: Implement
  return false;
}

////////////////////////////////////////////////////////////////
// ChunkDisassembler
////////////////////////////////////////////////////////////////

ChunkDisassembler::DisassemblyResult ChunkDisassembler::disassemble(
    const datablock_ptr_t& data,
    size_t chunk_size,
    encryptioncodec_ptr_t codec,
    CompressionType compression) {
  DisassemblyResult result;
  
  // TODO: Implement
  result.success = false;
  result.error_message = "Not implemented";
  
  return result;
}

ChunkDisassembler::DisassemblyResult ChunkDisassembler::disassembleFile(
    const file::Path& input_file,
    size_t chunk_size,
    encryptioncodec_ptr_t codec,
    CompressionType compression) {
  DisassemblyResult result;
  
  // TODO: Implement
  result.success = false;
  result.error_message = "Not implemented";
  
  return result;
}

bool ChunkDisassembler::writeChunksToDisk(
    const DisassemblyResult& result,
    const file::Path& output_dir,
    const std::string& base_filename) {
  // TODO: Implement
  return false;
}

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

bool validateChunkInfo(const ChunkManifest& info) {
  // TODO: Implement validation
  return true;
}

size_t estimateAssemblyMemoryUsage(const ChunkManifest& info) {
  // TODO: Implement memory estimation
  return info.total_size;
}

} // namespace ork::asset::catalog