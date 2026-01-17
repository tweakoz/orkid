////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shmtexture.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <cstring>
#include <chrono>
#include <stdexcept>

namespace bip = boost::interprocess;

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// ShmTextureData implementation
///////////////////////////////////////////////////////////////////////////////

uint8_t* ShmTextureData::frameData(int index) {
  // Frame data starts after header, aligned to 64 bytes
  size_t header_aligned = (sizeof(ShmTextureHeader) + 63) & ~size_t(63);
  return reinterpret_cast<uint8_t*>(this) + header_aligned + (index * header.frame_size);
}

const uint8_t* ShmTextureData::frameData(int index) const {
  size_t header_aligned = (sizeof(ShmTextureHeader) + 63) & ~size_t(63);
  return reinterpret_cast<const uint8_t*>(this) + header_aligned + (index * header.frame_size);
}

size_t ShmTextureData::totalSize(uint32_t frame_size) {
  size_t header_aligned = (sizeof(ShmTextureHeader) + 63) & ~size_t(63);
  return header_aligned + (SHMTEX_NUM_BUFFERS * frame_size);
}

uint32_t ShmTextureData::bytesPerPixel(EBufferFormat format) {
  switch (format) {
    case EBufferFormat::R8:
      return 1;
    case EBufferFormat::RGBA8:
      return 4;
    case EBufferFormat::NV12:
      // NV12 is 12 bits per pixel (1.5 bytes) - handled specially in calculateFrameSize
      return 1;
    default:
      return 0;
  }
}

uint32_t ShmTextureData::calculateFrameSize(uint32_t width, uint32_t height, EBufferFormat format) {
  switch (format) {
    case EBufferFormat::R8:
      return width * height;
    case EBufferFormat::RGBA8:
      return width * height * 4;
    case EBufferFormat::NV12:
      // Y plane: width * height
      // UV plane: width * (height/2) (interleaved U and V)
      return width * height + width * (height / 2);
    default:
      return 0;
  }
}

void ShmTextureData::initializeShmImage() {
  // Zero the header only (frame data doesn't need zeroing)
  std::memset(&header, 0, sizeof(ShmTextureHeader));
}

void ShmTextureData::uninitializeShmImage() {
  // Nothing to do for POD types
}

///////////////////////////////////////////////////////////////////////////////
// ShmTexProducer implementation
///////////////////////////////////////////////////////////////////////////////

ShmTexProducer::ptr_t ShmTexProducer::create(const ShmTexProducerConfig& config) {
  // Validate config
  if (config.width == 0 || config.height == 0) {
    throw std::invalid_argument("ShmTexProducer: width and height must be non-zero");
  }
  if (config.name.empty()) {
    throw std::invalid_argument("ShmTexProducer: name must not be empty");
  }

  // Validate format
  uint32_t frame_size = ShmTextureData::calculateFrameSize(config.width, config.height, config.format);
  if (frame_size == 0) {
    throw std::invalid_argument("ShmTexProducer: unsupported format (only R8, RGBA8, NV12 supported)");
  }

  // Use private constructor via shared_ptr
  return std::shared_ptr<ShmTexProducer>(new ShmTexProducer(config));
}

ShmTexProducer::ShmTexProducer(const ShmTexProducerConfig& config)
    : _name(config.name)
    , _width(config.width)
    , _height(config.height)
    , _format(config.format) {

  // Calculate stride (use provided or compute from format)
  if (config.stride > 0) {
    _stride = config.stride;
  } else {
    _stride = _width * ShmTextureData::bytesPerPixel(_format);
    // For NV12, stride is just width (Y plane stride)
    if (_format == EBufferFormat::NV12) {
      _stride = _width;
    }
  }

  // Calculate frame size and total SHM size
  uint32_t frame_size = ShmTextureData::calculateFrameSize(_width, _height, _format);
  _shm_size = ShmTextureData::totalSize(frame_size);

  // Prefixed name for easy identification/cleanup
  std::string shm_name = "ork.shmtex." + _name;

  try {
    // Try to open existing first
    bool need_recreate = false;
    try {
      _shm = std::make_unique<bip::shared_memory_object>(
          bip::open_only,
          shm_name.c_str(),
          bip::read_write);
      _is_creator = false;

      // Check if existing SHM has matching dimensions
      bip::mapped_region temp_region(*_shm, bip::read_only);
      auto* temp_header = static_cast<ShmTextureHeader*>(temp_region.get_address());
      if (temp_header->magic == SHMTEX_MAGIC &&
          (temp_header->width != _width || temp_header->height != _height ||
           temp_header->format != static_cast<uint32_t>(_format))) {
        // Dimensions or format mismatch - need to recreate
        need_recreate = true;
        _shm.reset();
      }
    } catch (const bip::interprocess_exception&) {
      // Doesn't exist, create new
      need_recreate = true;
    }

    if (need_recreate) {
      // Remove stale SHM if it exists
      bip::shared_memory_object::remove(shm_name.c_str());
      _shm = std::make_unique<bip::shared_memory_object>(
          bip::create_only,
          shm_name.c_str(),
          bip::read_write);
      _is_creator = true;
    }

    // Set size (safe to call on existing, will only grow)
    _shm->truncate(_shm_size);

    // Map the region
    _region = std::make_unique<bip::mapped_region>(*_shm, bip::read_write);
    _data = static_cast<ShmTextureData*>(_region->get_address());
    _header = &_data->header;

    if (_is_creator) {
      // Initialize the header
      _header->magic = SHMTEX_MAGIC;
      _header->version = SHMTEX_VERSION;
      _header->width = _width;
      _header->height = _height;
      _header->format = static_cast<uint32_t>(_format);
      _header->stride = _stride;
      _header->frame_size = frame_size;
      _header->num_buffers = SHMTEX_NUM_BUFFERS;

      // Initialize atomic fields
      _header->write_sequence.store(0, std::memory_order_relaxed);
      _header->read_sequence.store(0, std::memory_order_relaxed);
      _header->latest_buffer.store(0, std::memory_order_relaxed);

      for (int i = 0; i < SHMTEX_NUM_BUFFERS; i++) {
        _header->buffer_state[i].store(
            static_cast<uint32_t>(ShmTexBufferState::FREE),
            std::memory_order_relaxed);
        _header->frame_timestamp[i].store(0, std::memory_order_relaxed);
        _header->frame_sequence[i].store(0, std::memory_order_relaxed);
      }
    } else {
      // Validate existing SHM
      if (_header->magic != SHMTEX_MAGIC) {
        throw std::runtime_error("ShmTexProducer: invalid magic in existing SHM");
      }
      if (_header->version != SHMTEX_VERSION) {
        throw std::runtime_error("ShmTexProducer: version mismatch in existing SHM");
      }
      // Update our cached values from the SHM
      _width = _header->width;
      _height = _header->height;
      _stride = _header->stride;
      _format = static_cast<EBufferFormat>(_header->format);
    }

  } catch (const bip::interprocess_exception& e) {
    throw std::runtime_error(std::string("ShmTexProducer: ") + e.what());
  }
}

ShmTexProducer::~ShmTexProducer() {
  // Cancel any in-progress write
  if (_current_write_buffer >= 0) {
    cancelWrite();
  }

  // If we created it and there's no consumer, remove it
  if (_is_creator) {
    std::string shm_name = "ork.shmtex." + _name;
    bip::shared_memory_object::remove(shm_name.c_str());
  }
}

ShmTexProducer::WriteContext ShmTexProducer::beginWrite() {
  if (_current_write_buffer >= 0) {
    throw std::runtime_error("ShmTexProducer: beginWrite() called while write already in progress");
  }

  // Load latest buffer index
  uint32_t latest = _header->latest_buffer.load(std::memory_order_acquire);

  // Try to acquire a FREE buffer (prefer non-latest to avoid contention with consumer)
  int acquired = -1;

  // First pass: try non-latest FREE buffers
  for (int i = 0; i < SHMTEX_NUM_BUFFERS && acquired < 0; i++) {
    if (i == static_cast<int>(latest)) continue;

    uint32_t expected = static_cast<uint32_t>(ShmTexBufferState::FREE);
    if (_header->buffer_state[i].compare_exchange_strong(
            expected,
            static_cast<uint32_t>(ShmTexBufferState::WRITING),
            std::memory_order_acq_rel)) {
      acquired = i;
    }
  }

  // Second pass: try latest buffer if it's FREE
  if (acquired < 0) {
    uint32_t expected = static_cast<uint32_t>(ShmTexBufferState::FREE);
    if (_header->buffer_state[latest].compare_exchange_strong(
            expected,
            static_cast<uint32_t>(ShmTexBufferState::WRITING),
            std::memory_order_acq_rel)) {
      acquired = static_cast<int>(latest);
    }
  }

  // Third pass: overwrite a READY buffer (drop old frame)
  if (acquired < 0) {
    for (int i = 0; i < SHMTEX_NUM_BUFFERS && acquired < 0; i++) {
      uint32_t expected = static_cast<uint32_t>(ShmTexBufferState::READY);
      if (_header->buffer_state[i].compare_exchange_strong(
              expected,
              static_cast<uint32_t>(ShmTexBufferState::WRITING),
              std::memory_order_acq_rel)) {
        acquired = i;
      }
    }
  }

  // Should never happen with triple buffering unless consumer holds all buffers
  if (acquired < 0) {
    throw std::runtime_error("ShmTexProducer: no buffer available (this should not happen with triple buffering)");
  }

  _current_write_buffer = acquired;

  // Return write context
  WriteContext ctx;
  ctx.pixels = _data->frameData(acquired);
  ctx.width = _width;
  ctx.height = _height;
  ctx.stride = _stride;
  ctx.buffer_size = _header->frame_size;
  ctx.buffer_index = acquired;

  return ctx;
}

void ShmTexProducer::endWrite(uint64_t timestamp_ns) {
  if (_current_write_buffer < 0) {
    throw std::runtime_error("ShmTexProducer: endWrite() called without beginWrite()");
  }

  int buf = _current_write_buffer;
  _current_write_buffer = -1;

  // Increment write sequence
  uint64_t seq = _header->write_sequence.fetch_add(1, std::memory_order_relaxed) + 1;

  // Store metadata
  _header->frame_timestamp[buf].store(timestamp_ns, std::memory_order_relaxed);
  _header->frame_sequence[buf].store(seq, std::memory_order_relaxed);

  // Mark buffer as ready (release semantics ensures writes are visible)
  _header->buffer_state[buf].store(
      static_cast<uint32_t>(ShmTexBufferState::READY),
      std::memory_order_release);

  // Update latest buffer pointer
  _header->latest_buffer.store(static_cast<uint32_t>(buf), std::memory_order_release);

  _frames_written++;
}

void ShmTexProducer::cancelWrite() {
  if (_current_write_buffer < 0) {
    return;  // Nothing to cancel
  }

  int buf = _current_write_buffer;
  _current_write_buffer = -1;

  // Return buffer to FREE state
  _header->buffer_state[buf].store(
      static_cast<uint32_t>(ShmTexBufferState::FREE),
      std::memory_order_release);
}

uint64_t ShmTexProducer::framesWritten() const {
  return _frames_written;
}

uint64_t ShmTexProducer::framesDropped() const {
  // Frames dropped = frames written - frames read - buffers in flight
  uint64_t written = _header->write_sequence.load(std::memory_order_relaxed);
  uint64_t read = _header->read_sequence.load(std::memory_order_relaxed);
  if (written > read) {
    return written - read - 1;  // -1 because one frame may be pending
  }
  return 0;
}

uint32_t ShmTexProducer::width() const { return _width; }
uint32_t ShmTexProducer::height() const { return _height; }
uint32_t ShmTexProducer::stride() const { return _stride; }
EBufferFormat ShmTexProducer::format() const { return _format; }
const std::string& ShmTexProducer::name() const { return _name; }

///////////////////////////////////////////////////////////////////////////////
// ShmTexConsumer implementation
///////////////////////////////////////////////////////////////////////////////

ShmTexConsumer::ptr_t ShmTexConsumer::create(const ShmTexConsumerConfig& config) {
  if (config.name.empty()) {
    throw std::invalid_argument("ShmTexConsumer: name must not be empty");
  }

  return std::shared_ptr<ShmTexConsumer>(new ShmTexConsumer(config));
}

ShmTexConsumer::ShmTexConsumer(const ShmTexConsumerConfig& config)
    : _name(config.name) {

  // Prefixed name (must match producer)
  std::string shm_name = "ork.shmtex." + _name;

  try {
    // Open existing shared memory (producer must have created it)
    _shm = std::make_unique<bip::shared_memory_object>(
        bip::open_only,
        shm_name.c_str(),
        bip::read_write);

    // Map the region
    _region = std::make_unique<bip::mapped_region>(*_shm, bip::read_write);
    _data = static_cast<ShmTextureData*>(_region->get_address());
    _header = &_data->header;

    // Validate
    if (_header->magic != SHMTEX_MAGIC) {
      throw std::runtime_error("ShmTexConsumer: invalid magic (not a valid ShmTexture)");
    }
    if (_header->version != SHMTEX_VERSION) {
      throw std::runtime_error("ShmTexConsumer: version mismatch");
    }

  } catch (const bip::interprocess_exception& e) {
    throw std::runtime_error(std::string("ShmTexConsumer: ") + e.what());
  }
}

ShmTexConsumer::~ShmTexConsumer() {
  // Release any held buffer
  if (_current_read_buffer >= 0) {
    releaseFrame();
  }
}

bool ShmTexConsumer::tryGetFrame(FrameData& out) {
  if (_current_read_buffer >= 0) {
    // Already holding a frame - must release first
    return false;
  }

  if (!hasNewFrame()) {
    return false;
  }

  // Get the latest buffer
  uint32_t latest = _header->latest_buffer.load(std::memory_order_acquire);

  // Try to transition from READY to READING
  uint32_t expected = static_cast<uint32_t>(ShmTexBufferState::READY);
  if (!_header->buffer_state[latest].compare_exchange_strong(
          expected,
          static_cast<uint32_t>(ShmTexBufferState::READING),
          std::memory_order_acq_rel)) {
    // Buffer was grabbed by producer (being overwritten) - no frame this time
    return false;
  }

  _current_read_buffer = static_cast<int>(latest);

  // Populate frame data
  out.pixels = _data->frameData(latest);
  out.width = _header->width;
  out.height = _header->height;
  out.stride = _header->stride;
  out.format = static_cast<EBufferFormat>(_header->format);
  out.timestamp_ns = _header->frame_timestamp[latest].load(std::memory_order_relaxed);
  out.sequence = _header->frame_sequence[latest].load(std::memory_order_relaxed);

  return true;
}

bool ShmTexConsumer::currentFrame(FrameData& out) const {
  if (_current_read_buffer < 0) {
    return false;
  }

  int idx = _current_read_buffer;

  out.pixels = _data->frameData(idx);
  out.width = _header->width;
  out.height = _header->height;
  out.stride = _header->stride;
  out.format = static_cast<EBufferFormat>(_header->format);
  out.timestamp_ns = _header->frame_timestamp[idx].load(std::memory_order_relaxed);
  out.sequence = _header->frame_sequence[idx].load(std::memory_order_relaxed);

  return true;
}

void ShmTexConsumer::releaseFrame() {
  if (_current_read_buffer < 0) {
    return;
  }

  int buf = _current_read_buffer;
  _current_read_buffer = -1;

  // Update read sequence
  uint64_t seq = _header->frame_sequence[buf].load(std::memory_order_relaxed);
  _header->read_sequence.store(seq, std::memory_order_relaxed);

  // Mark buffer as free
  _header->buffer_state[buf].store(
      static_cast<uint32_t>(ShmTexBufferState::FREE),
      std::memory_order_release);
}

bool ShmTexConsumer::hasNewFrame() const {
  if (!_header) return false;
  uint64_t write_seq = _header->write_sequence.load(std::memory_order_acquire);
  uint64_t read_seq = _header->read_sequence.load(std::memory_order_relaxed);
  return write_seq > read_seq;
}

bool ShmTexConsumer::update(Context* ctx) {
  if (!ctx) {
    return false;
  }

  // Create texture on first use
  if (!_texture) {
    _texture = std::make_shared<Texture>();
    _texture->_debugName = "ShmTexConsumer:" + _name;
  }

  // Use TextureInterface to upload from SHM
  // This calls tryGetFrame() internally, uploads to GPU, and releases the frame
  auto* txi = ctx->TXI();
  bool uploaded = txi->initFromShm(_texture, shared_from_this());

  if (uploaded) {
    _frames_received++;

    // Track latency (note: frame data is no longer available after initFromShm)
    // We could track this in initFromShm, but for now just count frames
  }

  return uploaded;
}

texture_ptr_t ShmTexConsumer::texture() const {
  return _texture;
}

uint64_t ShmTexConsumer::framesReceived() const {
  return _frames_received;
}

uint64_t ShmTexConsumer::framesSkipped() const {
  if (!_header) return 0;
  uint64_t write_seq = _header->write_sequence.load(std::memory_order_relaxed);
  if (write_seq > _frames_received) {
    return write_seq - _frames_received;
  }
  return 0;
}

double ShmTexConsumer::averageLatencyMs() const {
  if (_frames_received == 0) return 0.0;
  return _latency_sum_ms / static_cast<double>(_frames_received);
}

bool ShmTexConsumer::isConnected() const {
  return _header != nullptr && _header->magic == SHMTEX_MAGIC;
}

uint32_t ShmTexConsumer::width() const {
  return _header ? _header->width : 0;
}

uint32_t ShmTexConsumer::height() const {
  return _header ? _header->height : 0;
}

EBufferFormat ShmTexConsumer::format() const {
  return _header ? static_cast<EBufferFormat>(_header->format) : EBufferFormat::NONE;
}

const std::string& ShmTexConsumer::name() const {
  return _name;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
