#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/util/crc.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>

// FFmpeg headers removed - now only included in movie_playback_ffmpeg.cpp

namespace ork::lev2 {

#if defined(__APPLE__)
extern bool _macosUseHIDPI;
#endif

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

struct CapturedMovieFrame {
  captureasync_ptr_t capture_future;  // Async GPU capture
  capturebuffer_ptr_t capture_buffer;  // The buffer being written to
  int frame_number;                    // For PTS calculation
  int expected_audio_samples;          // Samples to extract (e.g., 800 @ 60fps)
  double virtual_time;                 // For debugging/validation
};

///////////////////////////////////////////////////////////////////////////////////////////////

struct MovieCaptureSettings {
  int _width  = 0;
  int _height = 0;
  int _fps = 60;
  size_t _max_queue_size = 120;
  bool _audio_test_tone = false;
  rtbuffer_ptr_t _rtbuffer = nullptr;
  audiodevice_ptr_t _audiodevice = nullptr;
  std::string _filename = "output.mp4";
  std::string _preset_name = "medium";
};

///////////////////////////////////////////////////////////////////////////////////////////////

struct MovieCaptureContext {

  MovieCaptureContext(moviecapsettings_ptr_t settings);
  ~MovieCaptureContext();

  void init();
  void terminate();

  // Called from render thread to queue frame
  size_t enqueueFrame(captureasync_ptr_t future, capturebuffer_ptr_t buffer, int frame_num, int expected_samples);

  moviecapsettings_ptr_t _settings;

  int _frame  = 0;

  int _audio_sample_rate = 48000;
  int _audio_channels = 2;
  int64_t _audio_samples_written = 0;  // For PTS

  /////////////////////////////////////////////////////////////////////////////////////////
  // Encoding thread & queue
  /////////////////////////////////////////////////////////////////////////////////////////

  std::deque<CapturedMovieFrame> _frame_queue;
  std::mutex _queue_mutex;
  std::condition_variable _queue_cv;

  std::thread _encoding_thread;
  std::atomic<bool> _encoding_running{false};
  std::atomic<bool> _terminated{false};

private:
  void _startEncodingThread();
  void _stopEncodingThread();
  void _encodingThreadFunc();  // Main encoding loop
};

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

struct MovieAudioFrame {
  std::vector<float> _samples;  // interleaved samples
  int _sample_rate = 0;
  int _channels = 0;
  double _pts = 0.0;  // presentation timestamp
};

using movieaudioframe_ptr_t = std::shared_ptr<MovieAudioFrame>;
using audio_callback_t = std::function<void(movieaudioframe_ptr_t)>;

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

struct MovieAudioConfig {
  int _sample_rate = 0;
  int _num_channels = 0;
  std::string _codec_name;
  bool _valid = false;
  void dump() const;
};

using movieaudioconfig_ptr_t = std::shared_ptr<MovieAudioConfig>;

///////////////////////////////////////////////////////////////////////////////////////////////
// Movie Backend Selection
///////////////////////////////////////////////////////////////////////////////////////////////

enum class MovieBackend : uint64_t {
  CrcEnum(FFMPEG),          // CPU decode, cross-platform (default)
  CrcEnum(VIDEOTOOLBOX),    // macOS: Hardware decode → IOSurface → Vulkan
  CrcEnum(VAAPI),           // Linux AMD: Hardware decode → DMA-BUF → Vulkan
  CrcEnum(NVDEC)            // Linux NVIDIA: Hardware decode → CUDA → Vulkan
};

enum class MoviePixelFormat : uint64_t {
  CrcEnum(AUTO),            // Backend decides optimal format
  CrcEnum(YCBCR_NV12),      // 4:2:0 YCbCr (hardware native, most efficient)
  CrcEnum(YCBCR_P010),      // 4:2:0 YCbCr 10-bit (HDR)
  CrcEnum(RGB_RGBA8)        // RGB conversion at decode time
};

///////////////////////////////////////////////////////////////////////////////////////////////
// Backend Implementation Interface
///////////////////////////////////////////////////////////////////////////////////////////////

struct MovieBackendImpl {
  virtual ~MovieBackendImpl() = default;

  virtual bool init(const std::string& filename, MoviePixelFormat format) = 0;
  virtual void play() = 0;
  virtual void pause() = 0;
  virtual void stop() = 0;
  virtual void restart() = 0;

  // Video output
  virtual image_ptr_t currentImage() = 0;              // CPU path (FFmpeg)
  virtual texture_ptr_t currentTexture() = 0;          // GPU-direct path (native backends)
  virtual image_provider_ptr_t createImageProvider() = 0;
  virtual texture_provider_ptr_t createTextureProvider() = 0;

  // Audio
  virtual bool hasAudio() const = 0;
  virtual movieaudioconfig_ptr_t audioConfig() const = 0;
  virtual void setAudioCallback(audio_callback_t cb) = 0;

  // Playback info
  virtual double fps() const = 0;
  virtual double duration() const = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual int64_t currentFrameIndex() const = 0;

  // Diagnostic/metadata (optional - backends can return defaults)
  virtual int64_t bitRate() const { return 0; }
  virtual std::string formatName() const { return ""; }
  virtual std::string formatLongName() const { return ""; }
  virtual std::string videoCodecName() const { return ""; }
  virtual int64_t videoBitRate() const { return 0; }
};

using moviebackendimpl_ptr_t = std::shared_ptr<MovieBackendImpl>;

///////////////////////////////////////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////////////////////////////////////

struct MoviePlaybackContext;

///////////////////////////////////////////////////////////////////////////////////////////////
// Backend Factory Functions
///////////////////////////////////////////////////////////////////////////////////////////////

moviebackendimpl_ptr_t createFFmpegBackend(MoviePlaybackContext* ctx);

#if defined(__APPLE__)
moviebackendimpl_ptr_t createVideoToolboxBackend(MoviePlaybackContext* ctx);
#endif

#if defined(__linux__)
moviebackendimpl_ptr_t createVAAPIBackend(MoviePlaybackContext* ctx);
moviebackendimpl_ptr_t createNVDECBackend(MoviePlaybackContext* ctx);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////
// Movie Playback Context
///////////////////////////////////////////////////////////////////////////////////////////////

struct MoviePlaybackContext {

  enum class State : crc_enum_t {
    CrcEnum(STOPPED),
    CrcEnum(PLAYING),
    CrcEnum(PAUSED)
  };

  MoviePlaybackContext();
  ~MoviePlaybackContext();

  // Legacy init (100% compatible with existing code)
  void init(const std::string& filename);

  // New init with backend selection
  void init(const std::string& filename,
            MovieBackend backend,
            MoviePixelFormat format = MoviePixelFormat::AUTO);

  void play();
  void pause();
  void stop();
  void restart();

  // Legacy image provider (FFmpeg backend)
  image_provider_ptr_t createImageProvider();

  // New texture provider (native backends, GPU-direct)
  texture_provider_ptr_t createTextureProvider();

  void setAudioCallback(audio_callback_t cb);

  /////////////////////////////////////////////////////////////////////////////////////////
  // Playback state
  State _state = State::STOPPED;
  std::string _filename;
  MovieBackend _backend = MovieBackend::FFMPEG;
  MoviePixelFormat _pixel_format = MoviePixelFormat::AUTO;

  // Backend implementation (holds FFmpeg, VideoToolbox, VAAPI, or NVDEC backend)
  moviebackendimpl_ptr_t _backend_impl;

  // Threading (legacy - may be used by old code paths)
  std::thread _decode_thread;
  std::atomic<bool> _running{false};
  std::mutex _queue_mutex;
  std::condition_variable _queue_cv;

  // Frame queue
  std::deque<image_ptr_t> _frame_queue;
  size_t _max_queue_size = 30;

  // Timing
  double _fps = 0.0;
  double _frame_duration = 0.0;
  double _duration = 0.0;  // Total duration in seconds
  std::chrono::high_resolution_clock::time_point _playback_start;
  int64_t _current_frame_index = 0;

  // Video dimensions (populated from first decoded frame)
  int _video_width = 0;
  int _video_height = 0;

  // Audio
  audio_callback_t _audio_callback;
  std::deque<movieaudioframe_ptr_t> _audio_queue;
  std::mutex _audio_mutex;

  // Image provider
  image_provider_ptr_t _image_provider;
  image_ptr_t _current_image;
  std::mutex _image_mutex;
  svar64_t _audio_impl;

  // Audio configuration (populated from first decoded frame)
  movieaudioconfig_ptr_t _audio_config;

  /////////////////////////////////////////////////////////////////////////////////////////
private:
  void _decodeThreadFunc();
  void _cleanup();
};

using movieplayback_ptr_t = std::shared_ptr<MoviePlaybackContext>;

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
