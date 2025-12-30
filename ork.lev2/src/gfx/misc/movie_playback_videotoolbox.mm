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

// FFmpeg for audio decoding (hybrid approach: VideoToolbox for video, FFmpeg for audio)
// Rename FFmpeg's AVMediaType to avoid conflict with AVFoundation's AVMediaType
#define AVMediaType FFmpegAVMediaType
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#undef AVMediaType
// Use FFmpeg's media type enum via the renamed symbol
#define AVMEDIA_TYPE_AUDIO FFmpegAVMediaType::AVMEDIA_TYPE_AUDIO

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
  void _audioDecodeThreadFunc();
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

  // Deferred destruction queue: keep previous impls alive for N frames
  // Prevents race where MoltenVK still has pending commands referencing old VkImageView
  std::deque<vulkan::iosurfaceteximpl_ptr_t> _impl_graveyard;
  static constexpr size_t IMPL_GRAVEYARD_SIZE = 30;

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

  // FFmpeg audio decoding (hybrid approach)
  AVFormatContext* _ffmpeg_format_ctx = nullptr;
  AVCodecContext* _ffmpeg_audio_codec_ctx = nullptr;
  const AVCodec* _ffmpeg_audio_codec = nullptr;
  int _ffmpeg_audio_stream_idx = -1;
  std::thread _audio_decode_thread;
  std::atomic<bool> _audio_running{false};

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

    // Set up audio using FFmpeg (hybrid approach: VideoToolbox video, FFmpeg audio)
    // This runs in a separate thread from video decode

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

  // Initialize FFmpeg for audio decoding (hybrid approach)
  if (avformat_open_input(&_ffmpeg_format_ctx, filename.c_str(), nullptr, nullptr) >= 0) {
    if (avformat_find_stream_info(_ffmpeg_format_ctx, nullptr) >= 0) {
      // Find audio stream
      for (unsigned i = 0; i < _ffmpeg_format_ctx->nb_streams; i++) {
        if (_ffmpeg_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
          _ffmpeg_audio_stream_idx = i;
          break;
        }
      }

      if (_ffmpeg_audio_stream_idx >= 0) {
        AVStream* audio_stream = _ffmpeg_format_ctx->streams[_ffmpeg_audio_stream_idx];
        AVCodecParameters* audio_codecpar = audio_stream->codecpar;

        _ffmpeg_audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
        if (_ffmpeg_audio_codec) {
          _ffmpeg_audio_codec_ctx = avcodec_alloc_context3(_ffmpeg_audio_codec);
          if (_ffmpeg_audio_codec_ctx) {
            avcodec_parameters_to_context(_ffmpeg_audio_codec_ctx, audio_codecpar);
            int ret = avcodec_open2(_ffmpeg_audio_codec_ctx, _ffmpeg_audio_codec, nullptr);

            if (ret >= 0) {
              _audio_config = std::make_shared<MovieAudioConfig>();

              // Probe first audio frame to get actual parameters
              AVPacket packet;
              AVFrame* probe_frame = av_frame_alloc();
              bool found_audio_params = false;

              while (av_read_frame(_ffmpeg_format_ctx, &packet) >= 0) {
                if (packet.stream_index == _ffmpeg_audio_stream_idx) {
                  if (avcodec_send_packet(_ffmpeg_audio_codec_ctx, &packet) >= 0) {
                    if (avcodec_receive_frame(_ffmpeg_audio_codec_ctx, probe_frame) >= 0) {
                      _audio_config->_sample_rate = probe_frame->sample_rate;
                      _audio_config->_num_channels = probe_frame->ch_layout.nb_channels;

                      if (_audio_config->_sample_rate == 0) {
                        _audio_config->_sample_rate = _ffmpeg_audio_codec_ctx->sample_rate;
                      }
                      if (_audio_config->_num_channels == 0) {
                        _audio_config->_num_channels = _ffmpeg_audio_codec_ctx->ch_layout.nb_channels;
                      }
                      if (_audio_config->_sample_rate == 0) {
                        _audio_config->_sample_rate = audio_stream->time_base.den;
                        if (_audio_config->_sample_rate != 48000 && _audio_config->_sample_rate != 44100) {
                          printf("VideoToolbox: WARNING - unusual audio sample rate %d, defaulting to 44100\n",
                                 _audio_config->_sample_rate);
                          _audio_config->_sample_rate = 44100;
                        }
                      }

                      _audio_config->_codec_name = _ffmpeg_audio_codec->name;
                      _audio_config->_valid = true;
                      found_audio_params = true;
                      av_packet_unref(&packet);
                      break;
                    }
                  }
                }
                av_packet_unref(&packet);
              }

              av_frame_free(&probe_frame);

              // Seek back to start and flush codec
              av_seek_frame(_ffmpeg_format_ctx, _ffmpeg_audio_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
              avcodec_flush_buffers(_ffmpeg_audio_codec_ctx);

              if (found_audio_params) {
                printf("VideoToolbox: FFmpeg audio initialized: %d Hz, %d channels, codec=%s\n",
                       _audio_config->_sample_rate, _audio_config->_num_channels,
                       _audio_config->_codec_name.c_str());
              }
            }
          }
        }
      }
    }
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

  // Start video decode thread
  _decode_thread = std::thread([this]() {
    _decodeThreadFunc();
  });

  // Start audio decode thread (FFmpeg)
  if (_ffmpeg_audio_stream_idx >= 0 && _ffmpeg_audio_codec_ctx) {
    _audio_running = true;
    _audio_decode_thread = std::thread([this]() {
      _audioDecodeThreadFunc();
    });
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::pause() {
  _running = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::stop() {
  _running = false;
  _audio_running = false;

  if (_decode_thread.joinable()) {
    _decode_thread.join();
  }

  if (_audio_decode_thread.joinable()) {
    _audio_decode_thread.join();
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
      // REUSE: Same IOSurface ID means VideoToolbox reused the same IOSurface from pool
      // The existing IoSurfaceTexImpl.vkimage is still valid (same Metal texture backing)
      // DON'T create a new surface - that would destroy the old one and invalidate the VkImage
      handle = it->second;
      // Just use existing impl as-is - IOSurface content changed but VkImage is still valid
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

  // Decode ahead of playback by this amount (ensures frames ready before display)
  constexpr double DECODE_AHEAD_SECONDS = 0.1;  // 100ms lookahead
  constexpr size_t MAX_PENDING_FRAMES = 10;     // Don't decode too far ahead

  while (_running) {
    @autoreleasepool {
      // Check if we have too many pending frames - wait if buffer is full
      {
        std::lock_guard<std::mutex> lock(_frame_buffer_mutex);
        if (_pending_frames.size() >= MAX_PENDING_FRAMES) {
          // Buffer full, wait a bit before checking again
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          continue;
        }
      }

      // Pace decoding: decode ahead of real-time playback position
      auto now = std::chrono::high_resolution_clock::now();
      double elapsed = std::chrono::duration<double>(now - _playback_start).count();
      double decode_target = elapsed + DECODE_AHEAD_SECONDS;

      // Check highest PTS in pending frames to know what we've decoded up to
      double highest_decoded_pts = 0.0;
      {
        std::lock_guard<std::mutex> lock(_frame_buffer_mutex);
        if (!_pending_frames.empty()) {
          highest_decoded_pts = _pending_frames.rbegin()->pts;
        }
      }

      // If we're already decoded ahead enough, sleep briefly
      if (highest_decoded_pts > decode_target) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        continue;
      }

      // Read next compressed sample buffer
      CMSampleBufferRef sampleBuffer = [_video_output copyNextSampleBuffer];

      if (!sampleBuffer) {
        // End of stream or error
        AVAssetReaderStatus status = [_asset_reader status];
        printf("VideoToolbox: sampleBuffer is null, status=%ld\n", (long)status);
        if (status == AVAssetReaderStatusCompleted) {
          if (_looping) {
            // Reset for seamless loop
            printf("VideoToolbox: Looping - waiting for pending frames...\n");
            // Wait for all pending async decodes to complete before resetting
            VTDecompressionSessionWaitForAsynchronousFrames(_decompression_session);
            printf("VideoToolbox: Looping - resetting asset reader...\n");
            if (_resetAssetReaderForLoop()) {
              // Reset playback timing for new loop iteration
              _playback_start = std::chrono::high_resolution_clock::now();
              printf("VideoToolbox: Loop reset complete, continuing decode\n");
              // Also signal audio thread to resync (it will detect loop via its own EOF)
              continue;  // Continue decode loop with new asset reader
            } else {
              printf("VideoToolbox: Loop reset failed, stopping\n");
              _running = false;
              break;
            }
          } else {
            printf("VideoToolbox: End of stream reached\n");
            _running = false;
            break;
          }
        } else if (status == AVAssetReaderStatusFailed) {
          printf("VideoToolbox: Read error: %s\n",
                 [_asset_reader error].localizedDescription.UTF8String);
          _running = false;
          break;
        }
        break;  // Any other null sampleBuffer case - stop decode loop
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
    }
  }

  printf("VideoToolbox decode thread stopped\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Audio Decode Thread - FFmpeg audio decoding (hybrid approach)
///////////////////////////////////////////////////////////////////////////////////////////////

void VideoToolboxBackend::_audioDecodeThreadFunc() {
  printf("VideoToolbox audio decode thread started (FFmpeg)\n");

  AVPacket packet;
  AVFrame* frame = av_frame_alloc();

  // Use playback start time for PTS-based pacing
  auto audio_start = std::chrono::high_resolution_clock::now();

  while (_audio_running) {
    int ret = av_read_frame(_ffmpeg_format_ctx, &packet);
    if (ret < 0) {
      // End of file - loop back to start
      av_seek_frame(_ffmpeg_format_ctx, _ffmpeg_audio_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
      avcodec_flush_buffers(_ffmpeg_audio_codec_ctx);
      audio_start = std::chrono::high_resolution_clock::now();  // Reset timing
      continue;
    }

    // Only process audio packets
    if (packet.stream_index == _ffmpeg_audio_stream_idx) {
      ret = avcodec_send_packet(_ffmpeg_audio_codec_ctx, &packet);
      if (ret >= 0) {
        while (ret >= 0) {
          ret = avcodec_receive_frame(_ffmpeg_audio_codec_ctx, frame);
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
          }
          if (ret < 0) {
            break;
          }

          int channels = _ffmpeg_audio_codec_ctx->ch_layout.nb_channels;
          if (channels == 0) {
            AVCodecParameters* audio_codecpar = _ffmpeg_format_ctx->streams[_ffmpeg_audio_stream_idx]->codecpar;
            channels = audio_codecpar->ch_layout.nb_channels;
            if (channels == 0) {
              printf("VideoToolbox: WARNING - Cannot determine channel count, skipping audio frame\n");
              continue;
            }
          }

          auto audio_frame = std::make_shared<MovieAudioFrame>();
          audio_frame->_sample_rate = _audio_config->_sample_rate;
          audio_frame->_channels = channels;
          double pts = frame->pts * av_q2d(_ffmpeg_format_ctx->streams[_ffmpeg_audio_stream_idx]->time_base);
          audio_frame->_pts = pts;

          // PTS-based pacing: wait until this audio frame's time
          // Apply timeshift: positive = audio lags video (wait longer)
          double timeshift = _context->_audio_timeshift;
          auto now = std::chrono::high_resolution_clock::now();
          double elapsed = std::chrono::duration<double>(now - audio_start).count();
          double wait_time = (pts + timeshift) - elapsed;
          if (wait_time > 0.0 && wait_time < 1.0) {  // Sanity check: don't wait more than 1 second
            std::this_thread::sleep_for(std::chrono::duration<double>(wait_time));
          }

          audio_frame->_samples.resize(frame->nb_samples * audio_frame->_channels);

          if (frame->format == AV_SAMPLE_FMT_S16) {
            int16_t* samples = (int16_t*)frame->data[0];
            for (int i = 0; i < frame->nb_samples * audio_frame->_channels; i++) {
              audio_frame->_samples[i] = samples[i] / 32768.0f;
            }
          } else if (frame->format == AV_SAMPLE_FMT_FLT) {
            float* samples = (float*)frame->data[0];
            memcpy(audio_frame->_samples.data(), samples, frame->nb_samples * audio_frame->_channels * sizeof(float));
          } else if (frame->format == AV_SAMPLE_FMT_FLTP) {
            for (int i = 0; i < frame->nb_samples; i++) {
              for (int c = 0; c < audio_frame->_channels; c++) {
                if (frame->data[c]) {
                  float* channel_data = (float*)frame->data[c];
                  audio_frame->_samples[i * audio_frame->_channels + c] = channel_data[i];
                } else {
                  audio_frame->_samples[i * audio_frame->_channels + c] = 0.0f;
                }
              }
            }
          } else if (frame->format == AV_SAMPLE_FMT_S16P) {
            for (int i = 0; i < frame->nb_samples; i++) {
              for (int c = 0; c < audio_frame->_channels; c++) {
                if (frame->data[c]) {
                  int16_t* channel_data = (int16_t*)frame->data[c];
                  audio_frame->_samples[i * audio_frame->_channels + c] = channel_data[i] / 32768.0f;
                } else {
                  audio_frame->_samples[i * audio_frame->_channels + c] = 0.0f;
                }
              }
            }
          } else {
            printf("VideoToolbox: WARNING - Unsupported audio format %d\n", frame->format);
            continue;
          }

          // Call audio callback
          {
            std::lock_guard<std::mutex> lock(_audio_mutex);
            if (_audio_callback) {
              _audio_callback(audio_frame);
            }
          }
        }
      }
    }

    av_packet_unref(&packet);
  }

  av_frame_free(&frame);
  printf("VideoToolbox audio decode thread stopped\n");
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

  // WALL-CLOCK BASED FRAME SELECTION
  // Calculate current playback time and show the appropriate frame
  auto backend = static_cast<VideoToolboxBackend*>(_context->_backend_impl.get());

  std::lock_guard<std::mutex> lock(backend->_frame_buffer_mutex);

  // Wait until buffer has at least 2 frames before starting display (allows for B-frame reordering)
  if (!backend->_buffer_ready || backend->_pending_frames.size() < 2) {
    // Hold current frame if we have one, otherwise return nullptr
    return tex->_impl_2.isSet() ? tex : nullptr;
  }

  // Calculate current playback time from wall clock
  auto now = std::chrono::high_resolution_clock::now();
  double elapsed = std::chrono::duration<double>(now - backend->_playback_start).count();

  // Find the best frame to display:
  // - Frame with largest PTS that is <= current elapsed time
  // - This ensures we never show frames ahead of time, but catch up if behind
  vulkan::iosurfaceteximpl_ptr_t best_impl = nullptr;
  CVPixelBufferRef best_pixel_buffer = nullptr;
  double best_pts = -1.0;
  std::vector<decltype(backend->_pending_frames.begin())> frames_to_remove;

  for (auto it = backend->_pending_frames.begin(); it != backend->_pending_frames.end(); ++it) {
    if (it->pts <= elapsed) {
      // This frame's time has come (or passed)
      if (it->pts > best_pts) {
        // This is a better (more recent) frame to show
        best_impl = it->iosurface_impl;
        best_pixel_buffer = it->pixel_buffer;
        best_pts = it->pts;
      }
      frames_to_remove.push_back(it);
    }
  }

  if (!best_impl) {
    // No frame ready yet - hold current frame if we have one
    return tex->_impl_2.isSet() ? tex : nullptr;
  }

  // Remove all frames we've passed (including the one we're showing)
  // Release pixel buffers for frames we're skipping
  for (auto it : frames_to_remove) {
    if (it->pixel_buffer != best_pixel_buffer) {
      // This is a frame we're skipping - release its buffer
      CVPixelBufferRelease(it->pixel_buffer);
    }
    backend->_pending_frames.erase(it);
  }

  // DEFERRED RELEASE: Release previous frame's pixel buffer (allows VideoToolbox to recycle IOSurface)
  if (backend->_current_pixel_buffer && backend->_current_pixel_buffer != best_pixel_buffer) {
    CVPixelBufferRelease(backend->_current_pixel_buffer);
  }
  backend->_current_pixel_buffer = best_pixel_buffer;  // Take ownership (already retained in callback)

  // DEFERRED DESTRUCTION: Keep previous impl alive for N frames
  // This prevents MoltenVK from accessing a destroyed VkImageView in pending commands
  auto old_impl_opt = tex->_impl_2.tryAsShared<vulkan::IoSurfaceTexImpl>();
  if (old_impl_opt) {
    backend->_impl_graveyard.push_back(old_impl_opt.value());
    // Trim graveyard to max size
    while (backend->_impl_graveyard.size() > IMPL_GRAVEYARD_SIZE) {
      backend->_impl_graveyard.pop_front();
    }
  }

  // Update texture _impl_2
  tex->_impl_2.set<vulkan::iosurfaceteximpl_ptr_t>(best_impl);
  backend->_current_frame_index++;

  return tex;
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
  return _ffmpeg_audio_stream_idx >= 0 && _audio_config && _audio_config->_valid;
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

  // Clean up FFmpeg audio
  if (_ffmpeg_audio_codec_ctx) {
    avcodec_free_context(&_ffmpeg_audio_codec_ctx);
    _ffmpeg_audio_codec_ctx = nullptr;
  }
  if (_ffmpeg_format_ctx) {
    avformat_close_input(&_ffmpeg_format_ctx);
    _ffmpeg_format_ctx = nullptr;
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

  // Clear impl graveyard
  _impl_graveyard.clear();

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
