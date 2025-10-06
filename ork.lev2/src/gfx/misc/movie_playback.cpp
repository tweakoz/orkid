#include <ork/lev2/gfx/util/movie.inl>
#include <chrono>
#include <thread>
#include <string.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////////////////////

MoviePlaybackContext::MoviePlaybackContext() {
}

///////////////////////////////////////////////////////////////////////////////////////////////

MoviePlaybackContext::~MoviePlaybackContext() {
  stop();
  _cleanup();
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::init(const std::string& filename) {
  _filename = filename;

  // Open video file
  if (avformat_open_input(&_format_ctx, filename.c_str(), nullptr, nullptr) < 0) {
    printf("ERROR: Could not open video file: %s\n", filename.c_str());
    return;
  }

  // Retrieve stream information
  if (avformat_find_stream_info(_format_ctx, nullptr) < 0) {
    printf("ERROR: Could not find stream information\n");
    return;
  }

  // Find video stream
  for (unsigned i = 0; i < _format_ctx->nb_streams; i++) {
    if (_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && _video_stream_idx < 0) {
      _video_stream_idx = i;
    }
    if (_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && _audio_stream_idx < 0) {
      _audio_stream_idx = i;
    }
  }

  if (_video_stream_idx == -1) {
    printf("ERROR: Could not find video stream\n");
    return;
  }

  // Get video codec
  AVCodecParameters* video_codecpar = _format_ctx->streams[_video_stream_idx]->codecpar;
  _video_codec = avcodec_find_decoder(video_codecpar->codec_id);
  if (!_video_codec) {
    printf("ERROR: Unsupported video codec\n");
    return;
  }

  // Allocate video codec context
  _video_codec_ctx = avcodec_alloc_context3(_video_codec);
  if (!_video_codec_ctx) {
    printf("ERROR: Could not allocate video codec context\n");
    return;
  }

  // Copy codec parameters to context
  if (avcodec_parameters_to_context(_video_codec_ctx, video_codecpar) < 0) {
    printf("ERROR: Could not copy video codec parameters\n");
    return;
  }

  // Open video codec
  if (avcodec_open2(_video_codec_ctx, _video_codec, nullptr) < 0) {
    printf("ERROR: Could not open video codec\n");
    return;
  }

  // Setup video timing
  AVStream* video_stream = _format_ctx->streams[_video_stream_idx];
  _fps = av_q2d(video_stream->r_frame_rate);
  if (_fps <= 0) {
    _fps = av_q2d(video_stream->avg_frame_rate);
  }
  if (_fps <= 0) {
    _fps = 30.0; // fallback
  }
  _frame_duration = 1.0 / _fps;

  // Note: we defer sws_context creation until we decode the first frame
  // because codecpar dimensions may not be accurate for all codecs

  // Setup audio if available
  if (_audio_stream_idx >= 0) {
    AVCodecParameters* audio_codecpar = _format_ctx->streams[_audio_stream_idx]->codecpar;
    _audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
    if (_audio_codec) {
      _audio_codec_ctx = avcodec_alloc_context3(_audio_codec);
      if (_audio_codec_ctx) {
        avcodec_parameters_to_context(_audio_codec_ctx, audio_codecpar);
        avcodec_open2(_audio_codec_ctx, _audio_codec, nullptr);
      }
    }
  }

  printf("Opened video: %s (codec: %s, stream fps: %.2f)\n",
         filename.c_str(),
         _video_codec->name,
         _fps);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::play() {
  if (_state == State::PLAYING) {
    return;
  }

  if (_state == State::STOPPED) {
    // Reset timing
    _current_frame_index = 0;
    _playback_start = std::chrono::high_resolution_clock::now();
  } else if (_state == State::PAUSED) {
    // Resume from pause - adjust start time
    auto now = std::chrono::high_resolution_clock::now();
    auto frame_time = std::chrono::microseconds(int64_t(_current_frame_index * _frame_duration * 1000000));
    _playback_start = now - frame_time;
  }

  _state = State::PLAYING;
  _running = true;

  // Start decode thread if not already running
  if (!_decode_thread.joinable()) {
    _decode_thread = std::thread(&MoviePlaybackContext::_decodeThreadFunc, this);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::pause() {
  if (_state == State::PLAYING) {
    _state = State::PAUSED;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::stop() {
  _state = State::STOPPED;
  _running = false;

  // Wait for decode thread to finish
  if (_decode_thread.joinable()) {
    _queue_cv.notify_all();
    _decode_thread.join();
  }

  // Clear queues
  {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    _frame_queue.clear();
  }
  {
    std::lock_guard<std::mutex> lock(_audio_mutex);
    _audio_queue.clear();
  }

  // Seek back to start
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

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::restart() {
  stop();
  play();
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::setAudioCallback(audio_callback_t cb) {
  std::lock_guard<std::mutex> lock(_audio_mutex);
  _audio_callback = cb;
}

///////////////////////////////////////////////////////////////////////////////////////////////

image_provider_ptr_t MoviePlaybackContext::createImageProvider() {
  if (!_image_provider) {
    _image_provider = std::make_shared<ImageProvider>();
    _image_provider->_func = [this]() -> image_ptr_t {
      std::lock_guard<std::mutex> img_lock(_image_mutex);
      std::lock_guard<std::mutex> queue_lock(_queue_mutex);

      if (_state != State::PLAYING) {
        return _current_image;
      }

      // Calculate expected frame based on elapsed time
      auto now = std::chrono::high_resolution_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - _playback_start).count();
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

      // Return current frame (repeat if queue empty)
      return _current_image;
    };
  }
  return _image_provider;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::_decodeThreadFunc() {
  AVPacket packet;
  AVFrame* frame = av_frame_alloc();
  AVFrame* rgb_frame = nullptr;

  while (_running) {
    // Pause if not playing
    if (_state != State::PLAYING) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    // Check if queue is full
    {
      std::unique_lock<std::mutex> lock(_queue_mutex);
      if (_frame_queue.size() >= _max_queue_size) {
        _queue_cv.wait_for(lock, std::chrono::milliseconds(10));
        continue;
      }
    }

    // Read packet
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
        if (!_sws_context ||
            _video_codec_ctx->width != frame->width ||
            _video_codec_ctx->height != frame->height) {
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
            nullptr
          );
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
          rgb_frame = av_frame_alloc();
          rgb_frame->format = AV_PIX_FMT_RGB24;
          rgb_frame->width = frame->width;
          rgb_frame->height = frame->height;
          int alloc_ret = av_frame_get_buffer(rgb_frame, 1);
          if (alloc_ret < 0) {
            printf("ERROR: Could not allocate RGB frame buffer\n");
            continue;
          }
        }

        // Convert YUV to RGB
        sws_scale(_sws_context, frame->data, frame->linesize, 0, frame->height,
                 rgb_frame->data, rgb_frame->linesize);

        // Create image from RGB data
        auto img = std::make_shared<Image>();
        img->_width = frame->width;
        img->_height = frame->height;
        img->_format = EBufferFormat::RGB8;
        img->_data = std::make_shared<DataBlock>();

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

          // Create audio frame
          auto audio_frame = std::make_shared<MovieAudioFrame>();
          audio_frame->_sample_rate = frame->sample_rate;
          audio_frame->_channels = frame->ch_layout.nb_channels;
          audio_frame->_pts = frame->pts * av_q2d(_format_ctx->streams[_audio_stream_idx]->time_base);

          // Convert to float samples
          int num_samples = frame->nb_samples * audio_frame->_channels;
          audio_frame->_samples.resize(num_samples);

          // Simple conversion (assumes S16 format, may need swresample for other formats)
          if (frame->format == AV_SAMPLE_FMT_S16) {
            int16_t* samples = (int16_t*)frame->data[0];
            for (int i = 0; i < num_samples; i++) {
              audio_frame->_samples[i] = samples[i] / 32768.0f;
            }
          } else if (frame->format == AV_SAMPLE_FMT_FLT || frame->format == AV_SAMPLE_FMT_FLTP) {
            float* samples = (float*)frame->data[0];
            for (int i = 0; i < num_samples; i++) {
              audio_frame->_samples[i] = samples[i];
            }
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

  av_frame_free(&rgb_frame);
  av_frame_free(&frame);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::_cleanup() {
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

} // namespace ork::lev2
