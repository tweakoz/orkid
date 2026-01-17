////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/lev2_types.h>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <cstdint>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////

class ShmTexProducer;
class ShmTexConsumer;

using shmtexproducer_ptr_t = std::shared_ptr<ShmTexProducer>;
using shmtexconsumer_ptr_t = std::shared_ptr<ShmTexConsumer>;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////

static constexpr uint32_t SHMTEX_MAGIC = 0x53485458;  // 'SHTX'
static constexpr uint32_t SHMTEX_VERSION = 1;
static constexpr int SHMTEX_NUM_BUFFERS = 3;

///////////////////////////////////////////////////////////////////////////////
// BufferState - triple-buffer state machine
///////////////////////////////////////////////////////////////////////////////

enum class ShmTexBufferState : uint32_t {
  FREE = 0,      // Available for writing
  WRITING = 1,   // Producer is writing
  READY = 2,     // Contains valid frame, available for reading
  READING = 3    // Consumer is reading
};

///////////////////////////////////////////////////////////////////////////////
// ShmTextureHeader - control block for producer/consumer coordination
//
// Layout: Two cache lines (128 bytes)
//   - Line 0: Immutable configuration (read-only after init)
//   - Line 1: Atomic coordination fields
///////////////////////////////////////////////////////////////////////////////

struct ShmTextureHeader {
  // === Cache Line 0 (64 bytes) - Immutable after creation ===

  uint32_t magic;                     // SHMTEX_MAGIC
  uint32_t version;                   // SHMTEX_VERSION
  uint32_t width;                     // Image width in pixels
  uint32_t height;                    // Image height in pixels
  uint32_t format;                    // EBufferFormat enum value
  uint32_t stride;                    // Bytes per row (may include padding)
  uint32_t frame_size;                // Total bytes per frame
  uint32_t num_buffers;               // Always SHMTEX_NUM_BUFFERS (3)
  uint8_t reserved0[32];              // Pad to 64 bytes

  // === Cache Line 1+ (64+ bytes) - Producer/Consumer coordination ===

  // Monotonic frame counter (only producer writes)
  std::atomic<uint64_t> write_sequence;

  // Last consumed frame sequence (only consumer writes)
  std::atomic<uint64_t> read_sequence;

  // Index of most recent READY buffer (producer updates after write)
  std::atomic<uint32_t> latest_buffer;

  // Padding for alignment
  uint32_t _pad1;

  // Per-buffer state
  std::atomic<uint32_t> buffer_state[SHMTEX_NUM_BUFFERS];

  // Per-buffer timestamps (nanoseconds since epoch)
  std::atomic<uint64_t> frame_timestamp[SHMTEX_NUM_BUFFERS];

  // Per-buffer sequence numbers
  std::atomic<uint64_t> frame_sequence[SHMTEX_NUM_BUFFERS];
};

///////////////////////////////////////////////////////////////////////////////
// ShmTextureData - complete shared memory layout
//
// Contains header followed by triple frame buffers, aligned to 64 bytes.
///////////////////////////////////////////////////////////////////////////////

struct ShmTextureData {
  ShmTextureHeader header;
  // Frame data follows header, aligned to 64 bytes
  // Actual frame pixels are accessed via frameData() methods

  /// Get pointer to frame buffer at index (0, 1, or 2)
  uint8_t* frameData(int index);
  const uint8_t* frameData(int index) const;

  /// Calculate total SHM size needed for given frame size
  static size_t totalSize(uint32_t frame_size);

  /// Calculate bytes per pixel for a format
  static uint32_t bytesPerPixel(EBufferFormat format);

  /// Calculate frame size from dimensions and format
  static uint32_t calculateFrameSize(uint32_t width, uint32_t height, EBufferFormat format);

  /// ShmObject interface - called by creator
  void initializeShmImage();
  void uninitializeShmImage();
};

///////////////////////////////////////////////////////////////////////////////
// ShmTexProducerConfig
///////////////////////////////////////////////////////////////////////////////

struct ShmTexProducerConfig {
  std::string name;                              // Shared memory segment name
  uint32_t width = 0;                            // Image width (required)
  uint32_t height = 0;                           // Image height (required)
  EBufferFormat format = EBufferFormat::RGBA8;   // R8, RGBA8, or NV12
  uint32_t stride = 0;                           // Bytes per row (0 = auto)
};

///////////////////////////////////////////////////////////////////////////////
// ShmTexProducer - GPU-agnostic shared memory texture producer
//
// Can run in headless/server processes without any graphics context.
// Only dependencies: POSIX shared memory, atomics, memcpy.
///////////////////////////////////////////////////////////////////////////////

class ShmTexProducer {
public:
  using ptr_t = std::shared_ptr<ShmTexProducer>;

  /// Create a new producer (creates or attaches to SHM segment)
  static ptr_t create(const ShmTexProducerConfig& config);

  ~ShmTexProducer();

  // No copy
  ShmTexProducer(const ShmTexProducer&) = delete;
  ShmTexProducer& operator=(const ShmTexProducer&) = delete;

  /// Context returned by beginWrite()
  struct WriteContext {
    uint8_t* pixels;           // Direct pointer to write buffer
    uint32_t width;            // Image width
    uint32_t height;           // Image height
    uint32_t stride;           // Bytes per row
    size_t buffer_size;        // Total bytes available
    int buffer_index;          // Internal buffer index
  };

  /// Acquire buffer for writing (always succeeds with triple buffering)
  WriteContext beginWrite();

  /// Submit frame with timestamp (nanoseconds since epoch)
  void endWrite(uint64_t timestamp_ns);

  /// Cancel current write (buffer returns to FREE state)
  void cancelWrite();

  /// Statistics
  uint64_t framesWritten() const;
  uint64_t framesDropped() const;  // Consumer didn't read in time

  /// Properties
  uint32_t width() const;
  uint32_t height() const;
  uint32_t stride() const;
  EBufferFormat format() const;
  const std::string& name() const;

private:
  ShmTexProducer(const ShmTexProducerConfig& config);

  std::string _name;
  uint32_t _width = 0;
  uint32_t _height = 0;
  uint32_t _stride = 0;
  EBufferFormat _format = EBufferFormat::RGBA8;
  size_t _shm_size = 0;
  bool _is_creator = false;
  std::unique_ptr<boost::interprocess::shared_memory_object> _shm;
  std::unique_ptr<boost::interprocess::mapped_region> _region;
  ShmTextureData* _data = nullptr;
  ShmTextureHeader* _header = nullptr;
  int _current_write_buffer = -1;
  uint64_t _frames_written = 0;
};

///////////////////////////////////////////////////////////////////////////////
// ShmTexConsumerConfig
///////////////////////////////////////////////////////////////////////////////

struct ShmTexConsumerConfig {
  std::string name;              // Must match producer's name
};

///////////////////////////////////////////////////////////////////////////////
// ShmTexConsumer - shared memory texture consumer with GPU upload
//
// Reads frames from shared memory and uploads to GPU textures via
// the TextureInterface abstraction.
///////////////////////////////////////////////////////////////////////////////

class ShmTexConsumer : public std::enable_shared_from_this<ShmTexConsumer> {
public:
  using ptr_t = std::shared_ptr<ShmTexConsumer>;

  /// Create a new consumer (attaches to existing SHM segment)
  static ptr_t create(const ShmTexConsumerConfig& config);

  ~ShmTexConsumer();

  // No copy
  ShmTexConsumer(const ShmTexConsumer&) = delete;
  ShmTexConsumer& operator=(const ShmTexConsumer&) = delete;

  /// Frame data structure
  struct FrameData {
    const uint8_t* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    EBufferFormat format;
    uint64_t timestamp_ns;
    uint64_t sequence;
  };

  /// Poll for new frame and upload to GPU
  /// Returns true if new frame was uploaded
  bool update(Context* ctx);

  /// Get current texture (may be null if no frames received yet)
  texture_ptr_t texture() const;

  /// Acquire next frame from SHM (for manual control)
  bool tryGetFrame(FrameData& out);

  /// Get currently acquired frame (for TextureInterface to access)
  bool currentFrame(FrameData& out) const;

  /// Release acquired frame back to SHM
  void releaseFrame();

  /// Check if new frame is available
  bool hasNewFrame() const;

  /// Statistics
  uint64_t framesReceived() const;
  uint64_t framesSkipped() const;  // write_seq - read_seq - 1
  double averageLatencyMs() const;

  /// Connection state
  bool isConnected() const;

  /// Properties
  uint32_t width() const;
  uint32_t height() const;
  EBufferFormat format() const;
  const std::string& name() const;

private:
  ShmTexConsumer(const ShmTexConsumerConfig& config);

  std::string _name;
  std::unique_ptr<boost::interprocess::shared_memory_object> _shm;
  std::unique_ptr<boost::interprocess::mapped_region> _region;
  ShmTextureData* _data = nullptr;
  ShmTextureHeader* _header = nullptr;
  texture_ptr_t _texture;

  int _current_read_buffer = -1;
  uint64_t _frames_received = 0;
  uint64_t _last_sequence = 0;
  double _latency_sum_ms = 0.0;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
