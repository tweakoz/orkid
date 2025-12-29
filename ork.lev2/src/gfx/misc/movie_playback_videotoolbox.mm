////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/util/movie.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/image.h>
#include <ork/kernel/svariant.h>

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <VideoToolbox/VideoToolbox.h>
#import <CoreVideo/CoreVideo.h>
#import <IOSurface/IOSurface.h>

// Include Vulkan headers for VulkanExternalTextureImpl
#if defined(__APPLE__)
#include "../vulkan/headers/vulkan_ctx.h"  // Full Vulkan context header
#include "../vulkan/vk_gpu_surface.h"      // IoSurfaceTexImpl
#include <ork/lev2/gfx/texman.h>           // GpuExternalSurface full definition
#endif

// Factory function declared in vk_gpu_surface_iosurface.mm
namespace ork::lev2::vulkan {
  gpu_external_surface_ptr_t createGpuSurfaceFromCVPixelBuffer(CVPixelBufferRef pixelBuffer);
}

namespace ork::lev2 {

// IoSurfaceTexImpl and iosurfaceteximpl_ptr_t are defined in vk_gpu_surface.h

///////////////////////////////////////////////////////////////////////////////////////////////
// VideoToolbox Backend Implementation
///////////////////////////////////////////////////////////////////////////////////////////////

class VideoToolboxBackend : public MovieBackendImpl {
public:
  VideoToolboxBackend(MoviePlaybackContext* ctx);
  ~VideoToolboxBackend() override;

  bool init(const std::string& filename, MoviePixelFormat format) override;
  void play() override;
  void pause() override;
  void stop() override;
  void restart() override;

  image_ptr_t currentImage() override;
  texture_ptr_t currentTexture() override;
  texture_ptr_t texture() const override { return _decode_texture; }
  image_provider_ptr_t createImageProvider() override;
  texture_provider_ptr_t createTextureProvider() override;

  bool hasAudio() const override;
  movieaudioconfig_ptr_t audioConfig() const override;
  void setAudioCallback(audio_callback_t cb) override;

  double fps() const override { return _fps; }
  double duration() const override { return _duration; }
  int width() const override { return _video_width; }
  int height() const override { return _video_height; }
  int64_t currentFrameIndex() const override { return _current_frame_index; }

private:
  void _decodeThreadFunc();
  void _cleanup();
  static void _decompressionCallback(
    void* decompressionOutputRefCon,
    void* sourceFrameRefCon,
    OSStatus status,
    VTDecodeInfoFlags infoFlags,
    CVImageBufferRef imageBuffer,
    CMTime presentationTimeStamp,
    CMTime presentationDuration);

  MoviePlaybackContext* _context = nullptr;

  // AVFoundation / VideoToolbox
  AVAssetReader* _asset_reader = nullptr;
  AVAssetReaderTrackOutput* _video_output = nullptr;
  AVAssetReaderTrackOutput* _audio_output = nullptr;
  VTDecompressionSessionRef _decompression_session = nullptr;

  // Threading
  std::thread _decode_thread;
  std::atomic<bool> _running{false};
  std::mutex _queue_mutex;
  std::condition_variable _queue_cv;

  // PTS-based frame reordering buffer (fixes B-frame decode order issue)
  struct PendingFrame {
    vulkan::iosurfaceteximpl_ptr_t iosurface_impl;
    double pts;  // Presentation timestamp in seconds
    CVPixelBufferRef pixel_buffer;  // Retained reference for this frame instance

    bool operator<(const PendingFrame& other) const {
      return pts < other.pts;  // Sort by PTS (ascending)
    }
  };

  std::mutex _frame_buffer_mutex;
  std::set<PendingFrame> _pending_frames;  // Auto-sorted by PTS
  double _next_expected_pts = 0.0;  // Track next frame to display
  bool _buffer_ready = false;  // True once initial frames buffered
  CVPixelBufferRef _current_pixel_buffer = nullptr;  // Currently displayed frame's buffer (for deferred release)

  texture_ptr_t _decode_texture;  // Single texture shared between decode and render threads

  // IOSurface → VkImage mapping (VideoToolbox reuses IOSurfaces from pool)
  std::mutex _iosurface_map_mutex;
  std::unordered_map<IOSurfaceID, vulkan::iosurfaceteximpl_ptr_t> _iosurface_to_impl;  // Key: stable IOSurface ID, not pointer

  // Timing
  double _fps = 0.0;
  double _frame_duration = 0.0;
  double _duration = 0.0;
  std::chrono::high_resolution_clock::time_point _playback_start;
  int64_t _current_frame_index = 0;

  // Video dimensions
  int _video_width = 0;
  int _video_height = 0;

  // Audio (uses FFmpeg audio decoder)
  audio_callback_t _audio_callback;
  std::mutex _audio_mutex;
  movieaudioconfig_ptr_t _audio_config;

  // Current texture
  texture_ptr_t _current_texture;
  texture_provider_ptr_t _texture_provider;
  std::mutex _texture_mutex;

  // Pixel format
  MoviePixelFormat _pixel_format = MoviePixelFormat::AUTO;
  // TODO: Default to NV12 once multi-planar VK_EXT_metal_objects import is implemented
  OSType _cv_pixel_format = kCVPixelFormatType_32BGRA; // BGRA (single-plane, works with current import)

  // Looping support
  std::string _filename;  // Store for looping
  bool _looping = true;   // Enable looping by default

  // Helper to reset asset reader for looping (called from decode thread)
  bool _resetAssetReaderForLoop();
};

///////////////////////////////////////////////////////////////////////////////////////////////

VideoToolboxBackend::VideoToolboxBackend(MoviePlaybackContext* ctx)
    : _context(ctx) {
  // Create texture immediately so movie.texture is valid before init/play
  _decode_texture = std::make_shared<Texture>();
  _decode_texture->_source = ETextureSource::MOVIE;
}

///////////////////////////////////////////////////////////////////////////////////////////////

VideoToolboxBackend::~VideoToolboxBackend() {
  stop();
  _cleanup();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Init - Set up AVFoundation and VideoToolbox
///////////////////////////////////////////////////////////////////////////////////////////////

bool VideoToolboxBackend::init(const std::string& filename, MoviePixelFormat format) {
  _pixel_format = format;
  _filename = filename;  // Store for looping

  // Determine pixel format
  switch (format) {
    case MoviePixelFormat::YCBCR_NV12:
      _cv_pixel_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
      break;
    case MoviePixelFormat::YCBCR_P010:
      _cv_pixel_format = kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
      break;
    case MoviePixelFormat::RGB_RGBA8:
      _cv_pixel_format = kCVPixelFormatType_32BGRA;
      break;
    case MoviePixelFormat::AUTO:
      // TODO: Default to NV12 once multi-planar VK_EXT_metal_objects import is implemented
      _cv_pixel_format = kCVPixelFormatType_32BGRA; // BGRA (single-plane, works now)
      break;
  }

  @autoreleasepool {
    NSString* nsFilename = [NSString stringWithUTF8String:filename.c_str()];
    NSURL* fileURL = [NSURL fileURLWithPath:nsFilename];

    NSError* error = nil;
    AVAsset* asset = [AVAsset assetWithURL:fileURL];

    if (!asset) {
      printf("ERROR: Could not open video file: %s\n", filename.c_str());
      return false;
    }

    // Get video track
    NSArray<AVAssetTrack*>* videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
    if (videoTracks.count == 0) {
      printf("ERROR: No video track found\n");
      return false;
    }

    AVAssetTrack* videoTrack = videoTracks[0];

    // Get video properties
    CMFormatDescriptionRef formatDesc = (__bridge CMFormatDescriptionRef)videoTrack.formatDescriptions[0];
    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDesc);
    _video_width = dimensions.width;
    _video_height = dimensions.height;

    // Get duration and frame rate
    _duration = CMTimeGetSeconds(asset.duration);
    _fps = videoTrack.nominalFrameRate;
    if (_fps <= 0.0) {
      _fps = 30.0;  // Fallback
    }
    _frame_duration = 1.0 / _fps;

    printf("VideoToolbox: Opening %s (%dx%d @ %.2f fps, duration=%.2fs)\n",
           filename.c_str(), _video_width, _video_height, _fps, _duration);

    // Create asset reader
    _asset_reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
    if (error) {
      printf("ERROR: Could not create asset reader: %s\n", error.localizedDescription.UTF8String);
      return false;
    }

    // Configure video output to provide compressed samples (we'll decompress with VTDecompressionSession)
    NSDictionary* videoSettings = nil;  // nil = compressed samples

    _video_output = [[AVAssetReaderTrackOutput alloc] initWithTrack:videoTrack
                                                      outputSettings:videoSettings];
    _video_output.alwaysCopiesSampleData = NO;

    if ([_asset_reader canAddOutput:_video_output]) {
      [_asset_reader addOutput:_video_output];
    } else {
      printf("ERROR: Cannot add video output to asset reader\n");
      return false;
    }

    // TODO: Set up audio track using FFmpeg audio decoder (hybrid approach)
    // For now, skip audio

    printf("VideoToolbox: Will create texture pool after VTDecompressionSession outputs first frames\n");

    // Create VTDecompressionSession to decode into our pool
    VTDecompressionOutputCallbackRecord callbackRecord;
    callbackRecord.decompressionOutputCallback = _decompressionCallback;
    callbackRecord.decompressionOutputRefCon = this;

    // Configure decompression session to use our pixel buffer pool
    NSDictionary* destinationPixelBufferAttributes = @{
      (NSString*)kCVPixelBufferPixelFormatTypeKey: @(_cv_pixel_format),
      (NSString*)kCVPixelBufferWidthKey: @(_video_width),
      (NSString*)kCVPixelBufferHeightKey: @(_video_height),
      (NSString*)kCVPixelBufferIOSurfacePropertiesKey: @{},
      (NSString*)kCVPixelBufferMetalCompatibilityKey: @YES,
      (NSString*)kCVPixelBufferPoolMinimumBufferCountKey: @(2),  // Double-buffering
    };

    NSDictionary* sessionOptions = @{
      (NSString*)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES,
    };

    OSStatus vt_status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      formatDesc,
      (__bridge CFDictionaryRef)sessionOptions,
      (__bridge CFDictionaryRef)destinationPixelBufferAttributes,
      &callbackRecord,
      &_decompression_session
    );

    if (vt_status != noErr || !_decompression_session) {
      printf("ERROR: Failed to create VTDecompressionSession (status=%d)\n", vt_status);
      return false;
    }

    // Set minimum buffer count to control pool size (CRITICAL for reuse)
    CFNumberRef minBufferCount = (__bridge CFNumberRef)@(2);  // Double-buffering
    VTSessionSetProperty(
        _decompression_session,
        kVTDecompressionPropertyKey_OutputPoolRequestedMinimumBufferCount,
        minBufferCount
    );

    printf("VideoToolbox: VTDecompressionSession created with pool size 2 (double-buffering)\n");

    // Start reading
    if (![_asset_reader startReading]) {
      printf("ERROR: Could not start asset reader\n");
      return false;
    }

    printf("VideoToolbox backend initialized successfully (format=%s)\n",
           format == MoviePixelFormat::YCBCR_NV12 ? "NV12" :
           format == MoviePixelFormat::YCBCR_P010 ? "P010" : "BGRA");
  }

  // Set texture dimensions now that we know them
  _decode_texture->_width = _video_width;
  _decode_texture->_height = _video_height;
  _decode_texture->_depth = 1;
  _decode_texture->_num_mips = 1;

  // Create texture provider and store on texture for direct assignment path
  _texture_provider = std::make_shared<LambdaTextureProvider>([this]() -> texture_ptr_t {
    return currentTexture();
  });
  _decode_texture->_update_provider = _texture_provider;

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Play/Pause/Stop
///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::play() {
  if (_running) {
    return;  // Already playing
  }

  _running = true;
  _playback_start = std::chrono::high_resolution_clock::now();

  // Start decode thread
  _decode_thread = std::thread([this]() {
    _decodeThreadFunc();
  });
}

///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::pause() {
  _running = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::stop() {
  _running = false;

  if (_decode_thread.joinable()) {
    _decode_thread.join();
  }

  _current_frame_index = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::restart() {
  stop();

  // Reset asset reader
  @autoreleasepool {
    if (_asset_reader) {
      [_asset_reader cancelReading];
      _asset_reader = nil;
    }

    // Reinitialize (will create new asset reader)
    // For now, just call play() - full restart requires re-init
  }

  play();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// VTDecompressionSession Callback
///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::_decompressionCallback(
    void* decompressionOutputRefCon,
    void* sourceFrameRefCon,
    OSStatus status,
    VTDecodeInfoFlags infoFlags,
    CVImageBufferRef imageBuffer,
    CMTime presentationTimeStamp,
    CMTime presentationDuration) {

  if (status != noErr) {
    printf("VTB: Decompression error: %d\n", status);
    return;
  }

  double pts_seconds = CMTimeGetSeconds(presentationTimeStamp);

  if (!imageBuffer) {
    printf("VTB: Callback received null imageBuffer\n");
    return;
  }

  // Get the backend instance
  auto* backend = static_cast<VideoToolboxBackend*>(decompressionOutputRefCon);

  CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)imageBuffer;
  IOSurfaceRef iosurface = CVPixelBufferGetIOSurface(pixelBuffer);

  if (!iosurface) {
    printf("VTB: ERROR - decoded frame has no IOSurface!\n");
    return;
  }

  // Check if frame was dropped
  if (infoFlags & kVTDecodeInfo_FrameDropped) {
    return;
  }

  // Debug: Save first few frames as raw data to verify decoding
  static int debug_frame_count = 0;
  if (false) { //debug_frame_count < 5) {
    size_t width = IOSurfaceGetWidth(iosurface);
    size_t height = IOSurfaceGetHeight(iosurface);
    OSType pixelFormat = IOSurfaceGetPixelFormat(iosurface);
    printf("VTB: Raw export frame %d: %zux%zu format=0x%x\n", debug_frame_count, width, height, pixelFormat);

    // Verify pixel format is BGRA
    OrkAssert(pixelFormat == kCVPixelFormatType_32BGRA);
    printf("VTB: Pixel format verified as BGRA\n");

    kern_return_t lockResult = IOSurfaceLock(iosurface, kIOSurfaceLockReadOnly, NULL);
    OrkAssert(lockResult == kIOReturnSuccess);
    printf("VTB: IOSurface locked successfully\n");

    void* baseAddr = IOSurfaceGetBaseAddress(iosurface);
    OrkAssert(baseAddr != nullptr);
    printf("VTB: IOSurface baseAddr=%p\n", baseAddr);

    size_t bytesPerRow = IOSurfaceGetBytesPerRow(iosurface);
    printf("VTB: bytesPerRow=%zu (expected: width*4=%zu)\n", bytesPerRow, width * 4);

    // Create Image and copy IOSurface data
    Image img;
    img.initWithFormat(width, height, EBufferFormat::BGRA8);
    printf("VTB: Created Image %zux%zu\n", width, height);

    // Copy row by row (IOSurface may have padding)
    uint8_t* src = (uint8_t*)baseAddr;
    for (size_t y = 0; y < height; y++) {
      uint8_t* dst = img.pixel8(0, y);
      memcpy(dst, src, width * 4);
      src += bytesPerRow;
    }
    printf("VTB: Copied IOSurface data to Image\n");

    kern_return_t unlockResult = IOSurfaceUnlock(iosurface, kIOSurfaceLockReadOnly, NULL);
    OrkAssert(unlockResult == kIOReturnSuccess);
    printf("VTB: IOSurface unlocked successfully\n");

    // Write to PNG
    char filename[256];
    snprintf(filename, sizeof(filename), "/tmp/vtb_frame_%03d.png", debug_frame_count);
    img.writeToFile(file::Path(filename));
    printf("VTB: Wrote PNG to %s\n", filename);

    printf("VTB: PNG export frame %d complete!\n", debug_frame_count);
    debug_frame_count++;
  }

  // Check if we've seen this IOSurface before (VideoToolbox reuses from pool)
  // Use IOSurfaceGetID() as key - stable across different pointer references to same surface
  IOSurfaceID surface_id = IOSurfaceGetID(iosurface);

  vulkan::iosurfaceteximpl_ptr_t handle;
  {
    std::lock_guard<std::mutex> lock(backend->_iosurface_map_mutex);
    auto it = backend->_iosurface_to_impl.find(surface_id);
    if (it != backend->_iosurface_to_impl.end()) {
      // REUSE: We've seen this IOSurface before, reuse the VkImage
      handle = it->second;
      // Update surface reference (GpuExternalSurface manages the CVPixelBuffer)
      // Per-frame lifetime is managed via PendingFrame.pixel_buffer
      handle->surface = vulkan::createGpuSurfaceFromCVPixelBuffer(pixelBuffer);
    } else {
      // NEW: First time seeing this IOSurface, create wrapper
      handle = std::make_shared<vulkan::IoSurfaceTexImpl>();
      // Create GpuExternalSurface from CVPixelBuffer (wraps IOSurface)
      // Per-frame lifetime is managed via PendingFrame.pixel_buffer (retained in callback, released in currentTexture)
      // Once VkImage is created, MoltenVK's Metal texture holds its own reference
      handle->surface = vulkan::createGpuSurfaceFromCVPixelBuffer(pixelBuffer);

      // Store in map for reuse using stable IOSurfaceID
      backend->_iosurface_to_impl[surface_id] = handle;
    }
  }

  // Get the single decode texture (create if needed)
  auto texture = backend->_decode_texture;
  if (!texture) {
    texture = std::make_shared<Texture>();
    backend->_decode_texture = texture;
  }

  // Update texture dimensions
  texture->_width = handle->width();
  texture->_height = handle->height();
  texture->_depth = 1;
  texture->_num_mips = 1;
  texture->_source = ETextureSource::MOVIE;

  // PTS-BASED FRAME REORDERING
  // Insert frame into sorted buffer (handles B-frame decode order)
  {
    std::lock_guard<std::mutex> lock(backend->_frame_buffer_mutex);

    VideoToolboxBackend::PendingFrame pending;
    pending.iosurface_impl = handle;
    pending.pts = pts_seconds;
    // Retain CVPixelBuffer for THIS frame instance (keeps IOSurface alive until displayed)
    // Will be released when frame is replaced as "current" in currentTexture()
    CVPixelBufferRetain(pixelBuffer);
    pending.pixel_buffer = pixelBuffer;

    backend->_pending_frames.insert(pending);
    backend->_buffer_ready = true;  // Mark buffer as ready once we have at least one frame
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Decode Thread - Feeds compressed samples to VTDecompressionSession
///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::_decodeThreadFunc() {
  printf("VideoToolbox decode thread started\n");

  auto decode_start = std::chrono::high_resolution_clock::now();
  int64_t frames_decoded = 0;

  while (_running) {
    @autoreleasepool {
      // FPS-based pacing: sleep until it's time to decode the next frame
      auto now = std::chrono::high_resolution_clock::now();
      double elapsed = std::chrono::duration<double>(now - decode_start).count();
      double target_time = frames_decoded * _frame_duration;
      double sleep_time = target_time - elapsed;

      if (sleep_time > 0.0) {
        std::this_thread::sleep_for(std::chrono::duration<double>(sleep_time));
      }

      // Read next compressed sample buffer
      CMSampleBufferRef sampleBuffer = [_video_output copyNextSampleBuffer];

      if (!sampleBuffer) {
        // End of stream or error
        AVAssetReaderStatus status = [_asset_reader status];
        if (status == AVAssetReaderStatusCompleted) {
          if (_looping) {
            // Reset for seamless loop
            if (_resetAssetReaderForLoop()) {
              // Reset decode timing for new loop iteration
              decode_start = std::chrono::high_resolution_clock::now();
              frames_decoded = 0;
              continue;  // Continue decode loop
            } else {
              printf("VideoToolbox: Loop reset failed, stopping\n");
              _running = false;
            }
          } else {
            printf("VideoToolbox: End of stream reached\n");
            _running = false;
          }
        } else if (status == AVAssetReaderStatusFailed) {
          printf("VideoToolbox: Read error: %s\n",
                 [_asset_reader error].localizedDescription.UTF8String);
          _running = false;
        }
        break;
      }

      // Feed compressed sample to VTDecompressionSession
      // It will decode directly into one of our pooled IOSurfaces and call the callback
      VTDecodeFrameFlags decode_flags = kVTDecodeFrame_EnableAsynchronousDecompression;
      VTDecodeInfoFlags info_flags = 0;

      OSStatus decode_status = VTDecompressionSessionDecodeFrame(
        _decompression_session,
        sampleBuffer,
        decode_flags,
        NULL,  // sourceFrameRefCon
        &info_flags
      );

      if (decode_status != noErr) {
        printf("VTB: VTDecompressionSessionDecodeFrame failed: %d\n", decode_status);
      }

      CFRelease(sampleBuffer);
      frames_decoded++;

      // TODO: Audio decode callback (hybrid FFmpeg audio)
      if (_audio_callback) {
        // Extract audio from separate track if present
      }
    }
  }

  printf("VideoToolbox decode thread stopped\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Current Texture - Time-Based Playback
///////////////////////////////////////////////////////////////////////////////////////////////

texture_ptr_t VideoToolboxBackend::currentTexture() {
  // Return the single decode texture (with triple-buffered ring)
  auto tex = _decode_texture;
  if (!tex) {
    return nullptr;  // Not initialized yet
  }

  // PTS-BASED FRAME DRAINING
  // Pop frames from sorted buffer in correct temporal order
  auto backend = static_cast<VideoToolboxBackend*>(_context->_backend_impl.get());

  std::lock_guard<std::mutex> lock(backend->_frame_buffer_mutex);

  // Wait until buffer has at least 2 frames before starting display (allows for B-frame reordering)
  if (!backend->_buffer_ready || backend->_pending_frames.size() < 2) {
    return nullptr;
  }

  // Get frame with lowest PTS (first in sorted set)
  auto it = backend->_pending_frames.begin();
  if (it == backend->_pending_frames.end()) {
    return nullptr;
  }

  // Frame duration for 24fps video: ~0.04167 seconds
  const double FRAME_DURATION = 1.0 / 24.0;
  const double PTS_TOLERANCE = FRAME_DURATION * 0.5;  // Allow 0.5 frame tolerance

  double frame_pts = it->pts;
  double expected_pts = backend->_next_expected_pts;

  // Check if this frame's PTS is close to expected (within tolerance)
  bool pts_matches = std::abs(frame_pts - expected_pts) < PTS_TOLERANCE;

  if (pts_matches || backend->_next_expected_pts == 0.0) {
    // Pop frame and display it
    auto new_impl = it->iosurface_impl;
    CVPixelBufferRef new_pixel_buffer = it->pixel_buffer;
    backend->_pending_frames.erase(it);

    // DEFERRED RELEASE: Release previous frame's pixel buffer (allows VideoToolbox to recycle IOSurface)
    if (backend->_current_pixel_buffer) {
      CVPixelBufferRelease(backend->_current_pixel_buffer);
    }
    backend->_current_pixel_buffer = new_pixel_buffer;  // Take ownership (already retained in callback)

    // Update expected PTS for next frame
    backend->_next_expected_pts = frame_pts + FRAME_DURATION;

    // Update texture _impl_2
    tex->_impl_2.set<vulkan::iosurfaceteximpl_ptr_t>(new_impl);

    return tex;
  } else {
    // Frame PTS doesn't match - still waiting for correct frame
    return nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

image_ptr_t VideoToolboxBackend::currentImage() {
  // VideoToolbox backend is GPU-direct only - no CPU images
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Texture Provider
///////////////////////////////////////////////////////////////////////////////////////////////

texture_provider_ptr_t VideoToolboxBackend::createTextureProvider() {
  return _texture_provider;  // Created in init()
}

///////////////////////////////////////////////////////////////////////////////////////////////

image_provider_ptr_t VideoToolboxBackend::createImageProvider() {
  // VideoToolbox backend is GPU-direct only
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Audio (Hybrid FFmpeg Audio)
///////////////////////////////////////////////////////////////////////////////////////////////

bool VideoToolboxBackend::hasAudio() const {
  return _audio_config && _audio_config->_valid;
}

///////////////////////////////////////////////////////////////////////////////////////////////

movieaudioconfig_ptr_t VideoToolboxBackend::audioConfig() const {
  return _audio_config;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::setAudioCallback(audio_callback_t cb) {
  std::lock_guard<std::mutex> lock(_audio_mutex);
  _audio_callback = cb;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Cleanup
///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::_cleanup() {
  @autoreleasepool {
    if (_asset_reader) {
      [_asset_reader cancelReading];
      _asset_reader = nil;
    }

    _video_output = nil;
    _audio_output = nil;

    if (_decompression_session) {
      VTDecompressionSessionInvalidate(_decompression_session);
      CFRelease(_decompression_session);
      _decompression_session = nullptr;
    }
  }

  // Release current pixel buffer
  if (_current_pixel_buffer) {
    CVPixelBufferRelease(_current_pixel_buffer);
    _current_pixel_buffer = nullptr;
  }

  // Clean up PTS reordering buffer (release retained CVPixelBuffers and IoSurfaceTexImpl shared_ptrs)
  {
    std::lock_guard<std::mutex> lock(_frame_buffer_mutex);
    for (const auto& frame : _pending_frames) {
      if (frame.pixel_buffer) {
        CVPixelBufferRelease(frame.pixel_buffer);
      }
    }
    _pending_frames.clear();
    _buffer_ready = false;
  }

  // Clean up IOSurface → VkImage mapping
  {
    std::lock_guard<std::mutex> lock(_iosurface_map_mutex);
    _iosurface_to_impl.clear();
  }

  // Clean up decode textures
  _decode_texture.reset();
  _current_texture.reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Reset Asset Reader for Looping
///////////////////////////////////////////////////////////////////////////////////////////////

bool VideoToolboxBackend::_resetAssetReaderForLoop() {
  @autoreleasepool {
    // Cancel and release old asset reader
    if (_asset_reader) {
      [_asset_reader cancelReading];
      _asset_reader = nil;
    }
    _video_output = nil;

    // Recreate asset and reader from stored filename
    NSString* nsFilename = [NSString stringWithUTF8String:_filename.c_str()];
    NSURL* fileURL = [NSURL fileURLWithPath:nsFilename];

    NSError* error = nil;
    AVAsset* asset = [AVAsset assetWithURL:fileURL];

    if (!asset) {
      printf("VideoToolbox: Loop reset failed - could not reopen file\n");
      return false;
    }

    // Get video track
    NSArray<AVAssetTrack*>* videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
    if (videoTracks.count == 0) {
      printf("VideoToolbox: Loop reset failed - no video track\n");
      return false;
    }

    AVAssetTrack* videoTrack = videoTracks[0];

    // Create new asset reader
    _asset_reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
    if (error) {
      printf("VideoToolbox: Loop reset failed - could not create asset reader: %s\n",
             error.localizedDescription.UTF8String);
      return false;
    }

    // Configure video output (compressed samples)
    NSDictionary* videoSettings = nil;
    _video_output = [[AVAssetReaderTrackOutput alloc] initWithTrack:videoTrack
                                                      outputSettings:videoSettings];
    _video_output.alwaysCopiesSampleData = NO;

    if ([_asset_reader canAddOutput:_video_output]) {
      [_asset_reader addOutput:_video_output];
    } else {
      printf("VideoToolbox: Loop reset failed - cannot add video output\n");
      return false;
    }

    // Start reading
    if (![_asset_reader startReading]) {
      printf("VideoToolbox: Loop reset failed - could not start reading\n");
      return false;
    }

    // Reset PTS tracking for new loop iteration
    _next_expected_pts = 0.0;

    // Clear pending frames (release retained pixel buffers)
    {
      std::lock_guard<std::mutex> lock(_frame_buffer_mutex);
      for (const auto& frame : _pending_frames) {
        if (frame.pixel_buffer) {
          CVPixelBufferRelease(frame.pixel_buffer);
        }
      }
      _pending_frames.clear();
      _buffer_ready = false;
    }

    return true;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Factory Function
///////////////////////////////////////////////////////////////////////////////////////////////

moviebackendimpl_ptr_t createVideoToolboxBackend(MoviePlaybackContext* ctx) {
  return std::make_shared<VideoToolboxBackend>(ctx);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
