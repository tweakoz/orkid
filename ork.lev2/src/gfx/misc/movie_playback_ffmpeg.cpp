////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/util/movie.inl>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/util/ringbuffer.inl>
#include <chrono>
#include <thread>
#include <string.h>
#include <cmath>

extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////////////////////
// FFmpeg Backend Implementation
///////////////////////////////////////////////////////////////////////////////////////////////

class FFmpegBackend : public MovieBackendImpl {
public:
  FFmpegBackend(MoviePlaybackContext* ctx);
  ~FFmpegBackend() override;

  bool init(const std::string& filename, MoviePixelFormat format) override;
  void play() override;
  void pause() override;
  void stop() override;
  void restart() override;

  image_ptr_t currentImage() override;
  texture_ptr_t currentTexture() override;
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

  // Diagnostic/metadata
  int64_t bitRate() const override;
  std::string formatName() const override;
  std::string formatLongName() const override;
  std::string videoCodecName() const override;
  int64_t videoBitRate() const override;

private:
  void _decodeThreadFunc();
  void _cleanup();

  MoviePlaybackContext* _context = nullptr;

  // FFmpeg decoding pipeline (moved from MoviePlaybackContext)
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
  double _duration = 0.0;
  std::chrono::high_resolution_clock::time_point _playback_start;
  int64_t _current_frame_index = 0;

  // Video dimensions
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

  // Audio configuration
  movieaudioconfig_ptr_t _audio_config;
};

///////////////////////////////////////////////////////////////////////////////////////////////

FFmpegBackend::FFmpegBackend(MoviePlaybackContext* ctx)
    : _context(ctx) {
}

///////////////////////////////////////////////////////////////////////////////////////////////

FFmpegBackend::~FFmpegBackend() {
  stop();
  _cleanup();
}

///////////////////////////////////////////////////////////////////////////////////////////////
// This is the exact init logic from the original movie_playback.cpp
///////////////////////////////////////////////////////////////////////////////////////////////

bool FFmpegBackend::init(const std::string& filename, MoviePixelFormat format) {
  // Suppress swscaler warnings about no accelerated conversion
  av_log_set_level(AV_LOG_ERROR);

  // Open video file
  if (avformat_open_input(&_format_ctx, filename.c_str(), nullptr, nullptr) < 0) {
    printf("ERROR: Could not open video file: %s\n", filename.c_str());
    return false;
  }

  // Retrieve stream information
  if (avformat_find_stream_info(_format_ctx, nullptr) < 0) {
    printf("ERROR: Could not find stream information\n");
    return false;
  }

  // Dump format info to see what FFmpeg actually detected (to stderr)
  fprintf(stderr, "\n=== FFmpeg dump for %s ===\n", filename.c_str());
  av_dump_format(_format_ctx, 0, filename.c_str(), 0);
  fprintf(stderr, "=== End FFmpeg dump ===\n\n");

  // Capture duration - try multiple sources
  if (_format_ctx->duration != AV_NOPTS_VALUE) {
    _duration = _format_ctx->duration / (double)AV_TIME_BASE;
  }

  // Sometimes format duration is wrong, try to get it from streams
  for (unsigned i = 0; i < _format_ctx->nb_streams; i++) {
    AVStream* stream = _format_ctx->streams[i];
    if (stream->duration != AV_NOPTS_VALUE) {
      double stream_duration = stream->duration * av_q2d(stream->time_base);
      if (stream_duration > _duration) {
        _duration = stream_duration;
      }
    }
  }

  // Find video stream
  for (unsigned i = 0; i < _format_ctx->nb_streams; i++) {
    auto codec_type_str = av_get_media_type_string(_format_ctx->streams[i]->codecpar->codec_type);
    auto codec_name     = avcodec_get_name(_format_ctx->streams[i]->codecpar->codec_id);
    if (_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && _video_stream_idx < 0) {
      _video_stream_idx = i;
    }
    if (_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && _audio_stream_idx < 0) {
      _audio_stream_idx = i;
    }
  }

  if (_video_stream_idx == -1) {
    printf("ERROR: Could not find video stream\n");
    return false;
  }

  // Get video codec
  AVCodecParameters* video_codecpar = _format_ctx->streams[_video_stream_idx]->codecpar;
  _video_codec                      = avcodec_find_decoder(video_codecpar->codec_id);
  if (!_video_codec) {
    printf("ERROR: Unsupported video codec\n");
    return false;
  }

  // Allocate video codec context
  _video_codec_ctx = avcodec_alloc_context3(_video_codec);
  if (!_video_codec_ctx) {
    printf("ERROR: Could not allocate video codec context\n");
    return false;
  }

  // Copy codec parameters to context
  if (avcodec_parameters_to_context(_video_codec_ctx, video_codecpar) < 0) {
    printf("ERROR: Could not copy video codec parameters\n");
    return false;
  }

  // Open video codec
  if (avcodec_open2(_video_codec_ctx, _video_codec, nullptr) < 0) {
    printf("ERROR: Could not open video codec\n");
    return false;
  }

  // Setup video timing
  AVStream* video_stream = _format_ctx->streams[_video_stream_idx];

  // Prefer avg_frame_rate - it's more reliable for actual playback rate
  if (video_stream->avg_frame_rate.den > 0 && video_stream->avg_frame_rate.num > 0) {
    _fps = av_q2d(video_stream->avg_frame_rate);
  }

  // Fall back to r_frame_rate if avg not available
  if ((_fps <= 0 || std::isnan(_fps) || std::isinf(_fps)) && video_stream->r_frame_rate.den > 0 &&
      video_stream->r_frame_rate.num > 0) {
    _fps = av_q2d(video_stream->r_frame_rate);
  }

  // Try time_base reciprocal
  if ((_fps <= 0 || std::isnan(_fps) || std::isinf(_fps)) && video_stream->time_base.num > 0 && video_stream->time_base.den > 0) {
    _fps = (double)video_stream->time_base.den / (double)video_stream->time_base.num;
  }

  // Final fallback
  if (_fps <= 0 || std::isnan(_fps) || std::isinf(_fps)) {
    _fps = 30.0;
  }

  _frame_duration = 1.0 / _fps;

  // Setup audio if available
  if (_audio_stream_idx >= 0) {
    AVStream* audio_stream = _format_ctx->streams[_audio_stream_idx];
    AVCodecParameters* audio_codecpar = audio_stream->codecpar;

    _audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
    if (_audio_codec) {
      _audio_codec_ctx = avcodec_alloc_context3(_audio_codec);
      if (_audio_codec_ctx) {
        avcodec_parameters_to_context(_audio_codec_ctx, audio_codecpar);
        int ret = avcodec_open2(_audio_codec_ctx, _audio_codec, nullptr);

        if (ret >= 0) {
          _audio_config = std::make_shared<MovieAudioConfig>();

          if((audio_stream->time_base.num == 1) and (audio_stream->time_base.den==48000)){
            _audio_config->_sample_rate = 48000;
          }

          AVPacket packet;
          AVFrame* probe_frame = av_frame_alloc();
          bool found_audio_params = false;

          // Save current position
          int64_t original_pos = avio_tell(_format_ctx->pb);

          // Read packets until we find an audio frame
          while (av_read_frame(_format_ctx, &packet) >= 0) {
            if (packet.stream_index == _audio_stream_idx) {
              if (avcodec_send_packet(_audio_codec_ctx, &packet) >= 0) {
                if (avcodec_receive_frame(_audio_codec_ctx, probe_frame) >= 0) {
                  _audio_config->_sample_rate = probe_frame->sample_rate;
                  _audio_config->_num_channels = probe_frame->ch_layout.nb_channels;

                  if (_audio_config->_sample_rate == 0) {
                    _audio_config->_sample_rate = _audio_codec_ctx->sample_rate;
                  }
                  if (_audio_config->_num_channels == 0) {
                    _audio_config->_num_channels = _audio_codec_ctx->ch_layout.nb_channels;
                  }

                  if (_audio_config->_sample_rate == 0) {
                    _audio_config->_sample_rate = _audio_codec_ctx->sample_rate;
                    if (_audio_config->_num_channels == 0) {
                      _audio_config->_num_channels = _audio_codec_ctx->ch_layout.nb_channels;
                    }
                  }

                  if (_audio_config->_sample_rate == 0) {
                    _audio_config->_sample_rate = audio_stream->time_base.den;
                    if (_audio_config->_sample_rate != 48000) {
                      printf("WARNING: Audio stream time_base.den is also zero! Defaulting to 44100 Hz\n");
                      _audio_config->_sample_rate = 44100;
                    }
                  }

                  _audio_config->_codec_name = _audio_codec->name;
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
          av_seek_frame(_format_ctx, _audio_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
          avcodec_flush_buffers(_audio_codec_ctx);
        }
      }
    }
  }

  // Probe first video frame to get actual dimensions
  if (_video_stream_idx >= 0 && _video_codec_ctx) {
    AVPacket packet;
    AVFrame* probe_frame = av_frame_alloc();
    bool found_video_dims = false;

    // Read packets until we find a video frame
    while (av_read_frame(_format_ctx, &packet) >= 0) {
      if (packet.stream_index == _video_stream_idx) {
        if (avcodec_send_packet(_video_codec_ctx, &packet) >= 0) {
          if (avcodec_receive_frame(_video_codec_ctx, probe_frame) >= 0) {
            _video_width = probe_frame->width;
            _video_height = probe_frame->height;
            found_video_dims = true;
            av_packet_unref(&packet);
            break;
          }
        }
      }
      av_packet_unref(&packet);
    }

    av_frame_free(&probe_frame);

    // Seek back to start and flush codec
    av_seek_frame(_format_ctx, _video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(_video_codec_ctx);

    if (!found_video_dims) {
      _video_width = video_codecpar->width;
      _video_height = video_codecpar->height;
    }
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Play/Pause/Stop - Exact logic from original movie_playback.cpp
///////////////////////////////////////////////////////////////////////////////////////////////

void FFmpegBackend::play() {
  if (_context->_state == MoviePlaybackContext::State::PLAYING) {
    return;
  }

  if (_context->_state == MoviePlaybackContext::State::STOPPED) {
    _current_frame_index = 0;
    _playback_start      = std::chrono::high_resolution_clock::now();
  } else if (_context->_state == MoviePlaybackContext::State::PAUSED) {
    auto now        = std::chrono::high_resolution_clock::now();
    auto frame_time = std::chrono::microseconds(int64_t(_current_frame_index * _frame_duration * 1000000));
    _playback_start = now - frame_time;
  }

  _context->_state   = MoviePlaybackContext::State::PLAYING;
  _running = true;

  if (!_decode_thread.joinable()) {
    _decode_thread = std::thread(&FFmpegBackend::_decodeThreadFunc, this);
  }
}

void FFmpegBackend::pause() {
  if (_context->_state == MoviePlaybackContext::State::PLAYING) {
    _context->_state = MoviePlaybackContext::State::PAUSED;
  }
}

void FFmpegBackend::stop() {
  _context->_state   = MoviePlaybackContext::State::STOPPED;
  _running = false;

  if (_decode_thread.joinable()) {
    _queue_cv.notify_all();
    _decode_thread.join();
  }

  {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    _frame_queue.clear();
  }
  {
    std::lock_guard<std::mutex> lock(_audio_mutex);
    _audio_queue.clear();
  }

  if (_format_ctx) {
    av_seek_frame(_format_ctx, _video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
    if (_video_codec_ctx) {
      avcodec_flush_buffers(_video_codec_ctx);
    }
    if (_audio_codec_ctx) {
      avcodec_flush_buffers(_audio_codec_ctx);
    }
  }

  _current_frame_index = 0;
}

void FFmpegBackend::restart() {
  // Simple restart: stop and play
  // Audio cleanup is handled by MoviePlaybackContext::restart()
  stop();
  play();
}

///////////////////////////////////////////////////////////////////////////////////////////////

image_ptr_t FFmpegBackend::currentImage() {
  std::lock_guard<std::mutex> lock(_image_mutex);
  return _current_image;
}

texture_ptr_t FFmpegBackend::currentTexture() {
  // FFmpeg backend outputs images, not textures
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////

image_provider_ptr_t FFmpegBackend::createImageProvider() {
  if (!_image_provider) {
    _image_provider        = std::make_shared<ImageProvider>();
    _image_provider->_func = [this]() -> image_ptr_t {
      std::lock_guard<std::mutex> img_lock(_image_mutex);
      std::lock_guard<std::mutex> queue_lock(_queue_mutex);

      if (_context->_state != MoviePlaybackContext::State::PLAYING) {
        return _current_image;
      }

      // Calculate expected frame based on elapsed time
      auto now               = std::chrono::high_resolution_clock::now();
      auto elapsed           = std::chrono::duration_cast<std::chrono::microseconds>(now - _playback_start).count();
      int64_t expected_frame = int64_t((elapsed / 1000000.0) * _fps);

      // Skip old frames to catch up
      while (!_frame_queue.empty() && _current_frame_index < expected_frame) {
        _current_image = _frame_queue.front();
        _frame_queue.pop_front();
        _current_frame_index++;
        _queue_cv.notify_one();
      }

      // If we have the next frame ready, use it
      if (!_frame_queue.empty() && _current_frame_index == expected_frame) {
        _current_image = _frame_queue.front();
        _frame_queue.pop_front();
        _current_frame_index++;
        _queue_cv.notify_one();
      }

      return _current_image;
    };
  }
  return _image_provider;
}

texture_provider_ptr_t FFmpegBackend::createTextureProvider() {
  // FFmpeg backend outputs images, not textures
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////

bool FFmpegBackend::hasAudio() const {
  return _audio_stream_idx >= 0 && _audio_config && _audio_config->_valid;
}

movieaudioconfig_ptr_t FFmpegBackend::audioConfig() const {
  return _audio_config;
}

void FFmpegBackend::setAudioCallback(audio_callback_t cb) {
  std::lock_guard<std::mutex> lock(_audio_mutex);
  _audio_callback = cb;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Decode Thread - Exact logic from original movie_playback.cpp lines 635-835
///////////////////////////////////////////////////////////////////////////////////////////////

void FFmpegBackend::_decodeThreadFunc() {
  AVPacket packet;
  AVFrame* frame     = av_frame_alloc();
  AVFrame* rgb_frame = nullptr;

  while (_running) {
    if (_context->_state != MoviePlaybackContext::State::PLAYING) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    {
      std::unique_lock<std::mutex> lock(_queue_mutex);
      if (_frame_queue.size() >= _max_queue_size) {
        _queue_cv.wait_for(lock, std::chrono::milliseconds(10));
        continue;
      }
    }

    int ret = av_read_frame(_format_ctx, &packet);
    if (ret < 0) {
      // End of file - loop back to start
      av_seek_frame(_format_ctx, _video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
      avcodec_flush_buffers(_video_codec_ctx);
      if (_audio_codec_ctx) {
        avcodec_flush_buffers(_audio_codec_ctx);
      }
      continue;
    }

    // Process video packet
    if (packet.stream_index == _video_stream_idx) {
      ret = avcodec_send_packet(_video_codec_ctx, &packet);
      if (ret < 0) {
        av_packet_unref(&packet);
        continue;
      }

      while (ret >= 0) {
        ret = avcodec_receive_frame(_video_codec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        }
        if (ret < 0) {
          break;
        }

        // Create scaler if not already created or if frame dimensions changed
        if (!_sws_context || _video_codec_ctx->width != frame->width || _video_codec_ctx->height != frame->height) {
          if (_sws_context) {
            sws_freeContext(_sws_context);
          }
          _sws_context = sws_getContext(
              frame->width,
              frame->height,
              (AVPixelFormat)frame->format,
              frame->width,
              frame->height,
              AV_PIX_FMT_RGB24,
              SWS_BILINEAR,
              nullptr,
              nullptr,
              nullptr);
          if (!_sws_context) {
            printf("ERROR: Could not create scaler context\n");
            continue;
          }
        }

        // Allocate RGB frame if not already allocated or if size changed
        if (!rgb_frame || rgb_frame->width != frame->width || rgb_frame->height != frame->height) {
          if (rgb_frame) {
            av_frame_free(&rgb_frame);
          }
          rgb_frame         = av_frame_alloc();
          rgb_frame->format = AV_PIX_FMT_RGB24;
          rgb_frame->width  = frame->width;
          rgb_frame->height = frame->height;
          int alloc_ret     = av_frame_get_buffer(rgb_frame, 1);
          if (alloc_ret < 0) {
            printf("ERROR: Could not allocate RGB frame buffer\n");
            continue;
          }
        }

        // Convert YUV to RGB
        sws_scale(_sws_context, frame->data, frame->linesize, 0, frame->height, rgb_frame->data, rgb_frame->linesize);

        // Create image from RGB data
        auto img     = std::make_shared<Image>();
        img->_width  = frame->width;
        img->_height = frame->height;
        img->_format = EBufferFormat::RGB8;
        img->_data   = std::make_shared<DataBlock>();

        size_t data_size = frame->width * frame->height * 3;
        img->_data->allocateBlock(data_size);
        memcpy(img->_data->_storage.data(), rgb_frame->data[0], data_size);

        // Enqueue frame
        {
          std::lock_guard<std::mutex> lock(_queue_mutex);
          _frame_queue.push_back(img);
        }
      }
    }
    // Process audio packet
    else if (packet.stream_index == _audio_stream_idx && _audio_codec_ctx && _audio_callback) {
      ret = avcodec_send_packet(_audio_codec_ctx, &packet);
      if (ret >= 0) {
        while (ret >= 0) {
          ret = avcodec_receive_frame(_audio_codec_ctx, frame);
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
          }
          if (ret < 0) {
            break;
          }

          int channels = _audio_codec_ctx->ch_layout.nb_channels;
          if (channels == 0) {
            AVCodecParameters* audio_codecpar = _format_ctx->streams[_audio_stream_idx]->codecpar;
            channels                          = audio_codecpar->ch_layout.nb_channels;
            if (channels == 0) {
              printf("WARNING: Cannot determine channel count, skipping audio frame\n");
              continue;
            }
          }

          auto audio_frame          = std::make_shared<MovieAudioFrame>();
          audio_frame->_sample_rate = _audio_config->_sample_rate;
          audio_frame->_channels    = channels;
          audio_frame->_pts         = frame->pts * av_q2d(_format_ctx->streams[_audio_stream_idx]->time_base);

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
                  float* channel_data                                   = (float*)frame->data[c];
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
                  int16_t* channel_data                                 = (int16_t*)frame->data[c];
                  audio_frame->_samples[i * audio_frame->_channels + c] = channel_data[i] / 32768.0f;
                } else {
                  audio_frame->_samples[i * audio_frame->_channels + c] = 0.0f;
                }
              }
            }
          } else {
            printf("WARNING: Unsupported audio format %d\n", frame->format);
            continue;
          }

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

  av_frame_free(&rgb_frame);
  av_frame_free(&frame);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void FFmpegBackend::_cleanup() {
  if (_sws_context) {
    sws_freeContext(_sws_context);
    _sws_context = nullptr;
  }

  if (_video_codec_ctx) {
    avcodec_free_context(&_video_codec_ctx);
    _video_codec_ctx = nullptr;
  }

  if (_audio_codec_ctx) {
    avcodec_free_context(&_audio_codec_ctx);
    _audio_codec_ctx = nullptr;
  }

  if (_format_ctx) {
    avformat_close_input(&_format_ctx);
    _format_ctx = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Diagnostic/Metadata Methods
///////////////////////////////////////////////////////////////////////////////////////////////

int64_t FFmpegBackend::bitRate() const {
  if (_format_ctx) {
    return _format_ctx->bit_rate;
  }
  return 0;
}

std::string FFmpegBackend::formatName() const {
  if (_format_ctx && _format_ctx->iformat && _format_ctx->iformat->name) {
    return _format_ctx->iformat->name;
  }
  return "";
}

std::string FFmpegBackend::formatLongName() const {
  if (_format_ctx && _format_ctx->iformat && _format_ctx->iformat->long_name) {
    return _format_ctx->iformat->long_name;
  }
  return "";
}

std::string FFmpegBackend::videoCodecName() const {
  if (_video_codec && _video_codec->name) {
    return _video_codec->name;
  }
  return "";
}

int64_t FFmpegBackend::videoBitRate() const {
  if (_video_codec_ctx) {
    return _video_codec_ctx->bit_rate;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Factory function
///////////////////////////////////////////////////////////////////////////////////////////////

moviebackendimpl_ptr_t createFFmpegBackend(MoviePlaybackContext* ctx) {
  return std::make_shared<FFmpegBackend>(ctx);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

