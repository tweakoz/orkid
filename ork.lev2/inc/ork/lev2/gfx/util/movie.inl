#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>

extern "C" {
//#include <x264.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

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
///////////////////////////////////////////////////////////////////////////////////////////////

struct MovieCaptureContext {

  MovieCaptureContext();
  ~MovieCaptureContext();

  void init(int width, int height, audiodevice_ptr_t audio_dev);
  void terminate();

  // Called from render thread to queue frame
  size_t enqueueFrame(captureasync_ptr_t future, capturebuffer_ptr_t buffer, int frame_num, int expected_samples);

  std::string _filename;

  int _width  = 0;
  int _height = 0;
  int _frame  = 0;
  int _fps                       = 60;

  int _audio_sample_rate = 48000;
  int _audio_channels = 2;
  int64_t _audio_samples_written = 0;  // For PTS

  /////////////////////////////////////////////////////////////////////////////////////////
  // Encoding thread & queue
  /////////////////////////////////////////////////////////////////////////////////////////

  std::deque<CapturedMovieFrame> _frame_queue;
  std::mutex _queue_mutex;
  std::condition_variable _queue_cv;
  size_t _max_queue_size = 30;  // ~0.5 sec @ 60fps

  std::thread _encoding_thread;
  std::atomic<bool> _encoding_running{false};
  std::atomic<bool> _terminated{false};

  audiodevice_ptr_t _audio_device;  // Reference to extract samples

  /////////////////////////////////////////////////////////////////////////////////////////
  // Audio buffering (accumulate samples until codec frame size is reached)
  /////////////////////////////////////////////////////////////////////////////////////////

  std::vector<float> _audio_buffer_left;
  std::vector<float> _audio_buffer_right;

private:
  void _initVideoStream();
  void _initAudioStream();
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

struct MoviePlaybackContext {

  enum class State : crc_enum_t {
    CrcEnum(STOPPED),
    CrcEnum(PLAYING),
    CrcEnum(PAUSED)
  };

  MoviePlaybackContext();
  ~MoviePlaybackContext();

  void init(const std::string& filename);
  void play();
  void pause();
  void stop();
  void restart();
  image_provider_ptr_t createImageProvider();
  void setAudioCallback(audio_callback_t cb);

  /////////////////////////////////////////////////////////////////////////////////////////
  // Playback state
  State _state = State::STOPPED;
  std::string _filename;

  // FFmpeg decoding pipeline
  AVFormatContext* _format_ctx = nullptr;
  AVCodecContext* _video_codec_ctx = nullptr;
  AVCodecContext* _audio_codec_ctx = nullptr;
  const AVCodec* _video_codec = nullptr;
  const AVCodec* _audio_codec = nullptr;
  int _video_stream_idx = -1;
  int _audio_stream_idx = -1;
  struct SwsContext* _sws_context = nullptr;

  // Threading
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
