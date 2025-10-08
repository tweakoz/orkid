#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/targetinterfaces.h>

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

struct MovieCaptureContext {

  MovieCaptureContext();

  void init(int width, int height);
  /////////////////////////////////////////////////////////////////////////////////////////
  ~MovieCaptureContext();
  /////////////////////////////////////////////////////////////////////////////////////////
  bool encodeFrame(AVFrame* frame); // returns true if encoded and stream end reached
  void writeFrame(captureasync_ptr_t capbuf);
  void terminate();
  /////////////////////////////////////////////////////////////////////////////////////////
  int _width  = 0;
  int _height = 0;
  int _frame  = 0;
  std::string _filename;
  struct SwsContext* _swscontext = nullptr;
  AVOutputFormat* _format  = nullptr;
  const AVCodec* _codec          = nullptr;
  AVCodecContext* _encoder       = nullptr;
  AVFormatContext* _muxer        = nullptr;
  AVStream* _stream              = nullptr;
  AVFrame* _rgb_pic              = nullptr;
  AVFrame* _yuv_pic              = nullptr;
  int _got_output                = 0;
  int _fps                       = 60;
  AVPacket _avpacket;
  bool _terminated = false;
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
  std::chrono::high_resolution_clock::time_point _playback_start;
  int64_t _current_frame_index = 0;

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
