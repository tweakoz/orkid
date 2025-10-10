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
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace ork::lev2 {

struct AudioImpl {
  using ringbuffer_t = ork::RingBuffer<float>;
  using ringbuffer_ptr_t = std::shared_ptr<ringbuffer_t>;
  using producer_t = std::function<float()>;

  AudioImpl(MoviePlaybackContext* pb,
            audio::singularity::synth_ptr_t synth)
      : _playback(pb) {

  // Get audio config immediately - it's available after init()
  auto audio_config = _playback->_audio_config;
  if (audio_config and audio_config->_valid) {
    printf(
        "Movie audio detected: rate=%d Hz, channels=%d, codec=%s\n",
        audio_config->_sample_rate,
        audio_config->_num_channels,
        audio_config->_codec_name.c_str());
  }

  _accumulator.atomicOp([](ringbuffer_ptr_t& unlocked){
     unlocked = std::make_shared<ringbuffer_t>(96000); 
  });
  // 2 seconds @ 48kHz
  _source       = std::make_shared<lev2::StreamingAudioInputChunkSource>();
  _prgdata   = audio::singularity::createStreamingOscillatorProgramFromSource(_source,400.0);
  _prgdata->_name = "MoviePlaybackProgram";
  _playback->setAudioCallback([=](movieaudioframe_ptr_t audio_frame) {
    OrkAssert(audio_frame->_channels > 0);
    OrkAssert(audio_frame->_samples.size() > 0);

    switch (audio_frame->_channels) {
      case 1: { // mono
        auto chunk          = std::make_shared<lev2::AudioInputChunk>(audio_frame->_channels);
        chunk->_num_frames  = audio_frame->_samples.size() / audio_frame->_channels;
        chunk->_chunk_index = _source->_chunk_index++;
        for (int c = 0; c < audio_frame->_channels; c++) {
          auto& chan = chunk->_channels[c];
          chan.resize(chunk->_num_frames);
          for (int i = 0; i < chunk->_num_frames; i++) {
            chan[i] = audio_frame->_samples[i * audio_frame->_channels + c];
          }
        }
        _source->_inputqueue.push(chunk);
        break;
      }
      case 2: { // stereo
        // Convert stereo to mono and push to accumulator
        switch(audio_config->_sample_rate){
          case 48000: {
            size_t num_frames = audio_frame->_samples.size() / audio_frame->_channels;
            _mono_buffer.resize(num_frames);
            for (size_t i = 0; i < num_frames; i++) {
              float L = audio_frame->_samples[i * audio_frame->_channels + 0]; // L
              float R = audio_frame->_samples[i * audio_frame->_channels + 1]; // R
              _mono_buffer[i] = 0.5f * (L + R);
            }
            _accumulator.atomicOp([&](ringbuffer_ptr_t& unlocked){
              unlocked->push_many(_mono_buffer.data(), num_frames);
            });
            break;
          }
          case 44100: { // simple linear resample from 44.1k to 48k 
            size_t num_frames = audio_frame->_samples.size() / audio_frame->_channels;
            float ratio = 48000.0f / 44100.0f;
            auto s = audio_frame->_samples;
            for (size_t i = 0; i < num_frames; i++) {
              int j = i * audio_frame->_channels;
              float L = s[j + 0]; // L
              float R = s[j + 1]; // R
              float mono_sample = 0.5f * (L + R);
              // Push multiple samples based on ratio
              _phase_accum += ratio;
              _mono_buffer.clear();
              while (_phase_accum >= 1.0f) {
                // Simple linear interpolation
                float fi = fmod(_phase_accum, 1.0f);
                float interp = fi - 1.0f;
                if(interp<0.0f) interp=0.0f;
                if(interp>1.0f) interp=1.0f;
                float sample_to_push = (1.0f - interp) * _prev_sample + interp * mono_sample;
                _mono_buffer.push_back(sample_to_push);
                _phase_accum -= 1.0f;
              }
              _accumulator.atomicOp([&](ringbuffer_ptr_t& unlocked){
                unlocked->push_many(_mono_buffer.data(), _mono_buffer.size());
              });
              _prev_sample = mono_sample;
            }
            break;
          }
          default:
            OrkAssert(false); // resampler not implemented yet
           break;
        }
        break;
      }
      case 6: { // 5.1

        // Convert 5.1 to mono and push to accumulator
        size_t num_frames = audio_frame->_samples.size() / audio_frame->_channels;
        _mono_buffer.resize(num_frames);
        for (size_t i = 0; i < num_frames; i++) {
          // Extract L channel only
          float L = audio_frame->_samples[i * audio_frame->_channels + 0]; // L
          float R = audio_frame->_samples[i * audio_frame->_channels + 1]; // R
          float C = audio_frame->_samples[i * audio_frame->_channels + 2]; //
          _mono_buffer[i] = 0.3333f * (L + R + C);
        }
        _accumulator.atomicOp([&](ringbuffer_ptr_t& unlocked){
          unlocked->push_many(_mono_buffer.data(), num_frames);
        });
        break;
      }
    }
  });

  _timer = std::make_shared<Timer>();
  _timer->Start();
  _audio_thread = std::make_shared<ork::Thread>("MovieAudioThread");
  std::vector<float> local_buffer(4096);
  _audio_thread->start([this](anyp thr_data) {
    size_t number_of_samples_sent = 0;
    while (true) {

      double elapsed = _timer->SecsSinceStart();
      // LOCKED at 48kHz until resampler is added
      size_t target_samples = size_t(elapsed * 48000.0);
      if (target_samples > number_of_samples_sent) {
        size_t samples_to_send = target_samples - number_of_samples_sent;
        while (samples_to_send > 0) {
          size_t chunk_size = std::min(samples_to_send, size_t(1024));
          auto chunk        = std::make_shared<lev2::AudioInputChunk>(1);
          chunk->_num_frames  = chunk_size;
          chunk->_chunk_index = _source->_chunk_index++;
          auto& chan          = chunk->_channels[0];
          chan.resize(chunk_size);
          _accumulator.atomicOp([&](ringbuffer_ptr_t& unlocked){
            if(unlocked->size()<chunk_size){
              if(0)printf("MOV UNDERFLOW!\n");
              for (size_t i = 0; i < chunk_size; i++) {
                chan[i] = 0.0f;
              }
            } else {
                unlocked->pop_many(chan.data(), chunk_size);
            }
          });
          number_of_samples_sent += chunk_size;
          samples_to_send -= chunk_size;
          _source->_inputqueue.push(chunk);
        }
      }
      else {
        ::usleep(100);
      }
    }
  });

  }

  MoviePlaybackContext* _playback;
  audio::singularity::prgdata_ptr_t _prgdata;
  LockedResource<ringbuffer_ptr_t> _accumulator;
  lev2::audiostreaminginputchunk_source_ptr_t _source;
  std::vector<float> _mono_buffer;
  thread_ptr_t _audio_thread;
  timer_ptr_t _timer;
  producer_t _producer;
  float _prev_sample = 0.0f;
  float _phase_accum = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////////////////////

MoviePlaybackContext::MoviePlaybackContext() {
}

///////////////////////////////////////////////////////////////////////////////////////////////

MoviePlaybackContext::~MoviePlaybackContext() {
  stop();
  _cleanup();
}

  ///////////////////////////////////////////////////////////////////////////////////////////////

  void MovieAudioConfig::dump() const {
    printf("MovieAudioConfig: sample_rate: %d channels: %d codec_name: %s valid: %d\n", _sample_rate, _num_channels, _codec_name.c_str(), _valid);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::init(const std::string& filename) {
  _filename = filename;

  // Suppress swscaler warnings about no accelerated conversion
  av_log_set_level(AV_LOG_ERROR);

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
    printf( "Stream %d: codec_type=%d:%s:%s\n", i, _format_ctx->streams[i]->codecpar->codec_type, codec_type_str,codec_name);
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
  _video_codec                      = avcodec_find_decoder(video_codecpar->codec_id);
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

  // Note: we defer sws_context creation until we decode the first frame
  // because codecpar dimensions may not be accurate for all codecs




  // Setup audio if available
  if (_audio_stream_idx >= 0) {
    AVStream* audio_stream = _format_ctx->streams[_audio_stream_idx];
    AVCodecParameters* audio_codecpar = audio_stream->codecpar;

    //printf("DEBUG: Audio time_base num: %d den: %d\n", audio_stream->time_base.num, audio_stream->time_base.den);
    //printf("DEBUG: Audio frame_rate num: %d den: %d\n", audio_stream->avg_frame_rate.num, audio_stream->avg_frame_rate.den);

    _audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
    if (_audio_codec) {
      _audio_codec_ctx = avcodec_alloc_context3(_audio_codec);
      if (_audio_codec_ctx) {
        avcodec_parameters_to_context(_audio_codec_ctx, audio_codecpar);
        int ret = avcodec_open2(_audio_codec_ctx, _audio_codec, nullptr);

        if (ret >= 0) {
          // For AAC and other codecs, codecpar may not have sample_rate populated
          // We need to decode one frame to get the actual parameters
          
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
                  // Got a frame! Extract the real parameters
                  //printf("DEBUG PROBE: frame->sample_rate=%d, nb_samples=%d, format=%d\n",
                  //       probe_frame->sample_rate, probe_frame->nb_samples, probe_frame->format);
                  //printf("DEBUG PROBE: ch_layout.nb_channels=%d\n", probe_frame->ch_layout.nb_channels);
                  //printf("DEBUG PROBE: codec_ctx->sample_rate=%d, codec_ctx->ch_layout.nb_channels=%d\n",
                  //       audio_codecpar->sample_rate, _audio_codec_ctx->ch_layout.nb_channels);
                  //printf("DEBUG PROBE: codec_ctx->profile=%d, extradata_size=%d\n",
                  //       _audio_codec_ctx->profile, _audio_codec_ctx->extradata_size);

                  // Check if sample_fmt gives us a clue
                  //printf("DEBUG PROBE: codec_ctx->sample_fmt=%d (%s)\n",
                  //       _audio_codec_ctx->sample_fmt,
                  //       av_get_sample_fmt_name(_audio_codec_ctx->sample_fmt));

                  _audio_config->_sample_rate = probe_frame->sample_rate;
                  _audio_config->_num_channels = probe_frame->ch_layout.nb_channels;

                  // If frame doesn't have it, try codec context
                  if (_audio_config->_sample_rate == 0) {
                    _audio_config->_sample_rate = _audio_codec_ctx->sample_rate;
                  }
                  if (_audio_config->_num_channels == 0) {
                    _audio_config->_num_channels = _audio_codec_ctx->ch_layout.nb_channels;
                  }

                  // After decoding, check if codec_ctx was updated by the decoder
                  if (_audio_config->_sample_rate == 0) {
                    //printf("DEBUG AFTER DECODE: codec_ctx->sample_rate=%d, codec_ctx->ch_layout.nb_channels=%d\n",
                    //       _audio_codec_ctx->sample_rate, _audio_codec_ctx->ch_layout.nb_channels);

                    // Decoder may have populated it now
                    _audio_config->_sample_rate = _audio_codec_ctx->sample_rate;
                    if (_audio_config->_num_channels == 0) {
                      _audio_config->_num_channels = _audio_codec_ctx->ch_layout.nb_channels;
                    }
                  }

                  // If STILL zero after all that, something is seriously wrong
                  if (_audio_config->_sample_rate == 0) {
                    //printf("ERROR: Could not determine audio sample rate from any source!\n");
                    //printf("DEBUG: Audio time_base num: %d den: %d\n", audio_stream->time_base.num, audio_stream->time_base.den);
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

          if (found_audio_params) {
            printf(
                "Audio stream: codec=%s, sample_rate=%d Hz, channels=%d (detected from frame)\n",
                _audio_config->_codec_name.c_str(),
                _audio_config->_sample_rate,
                _audio_config->_num_channels);
          } else {
            printf("WARNING: Could not detect audio parameters\n");
          }
        }
      }
    }
  }

  // Probe first video frame to get actual dimensions
  if (_video_stream_idx >= 0 && _video_codec_ctx) {
    AVPacket packet;
    AVFrame* probe_frame = av_frame_alloc();
    bool found_video_dims = false;

    // Save current position
    int64_t original_pos = avio_tell(_format_ctx->pb);

    // Read packets until we find a video frame
    while (av_read_frame(_format_ctx, &packet) >= 0) {
      if (packet.stream_index == _video_stream_idx) {
        if (avcodec_send_packet(_video_codec_ctx, &packet) >= 0) {
          if (avcodec_receive_frame(_video_codec_ctx, probe_frame) >= 0) {
            // Got a frame! Extract dimensions
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

    if (found_video_dims) {
      printf("Video dimensions: %dx%d (detected from frame)\n", _video_width, _video_height);
    } else {
      printf("WARNING: Could not detect video dimensions from frame\n");
      // Fallback to codecpar if available
      _video_width = video_codecpar->width;
      _video_height = video_codecpar->height;
    }
  }

  printf(
      "Opened video: %s (codec: %s, fps: %.2f, r_frame_rate: %d/%d, avg_frame_rate: %d/%d)\n",
      filename.c_str(),
      _video_codec->name,
      _fps,
      video_stream->r_frame_rate.num,
      video_stream->r_frame_rate.den,
      video_stream->avg_frame_rate.num,
      video_stream->avg_frame_rate.den);
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::play() {
  if (_state == State::PLAYING) {
    return;
  }

  if (_state == State::STOPPED) {
    // Reset timing
    _current_frame_index = 0;
    _playback_start      = std::chrono::high_resolution_clock::now();
  } else if (_state == State::PAUSED) {
    // Resume from pause - adjust start time
    auto now        = std::chrono::high_resolution_clock::now();
    auto frame_time = std::chrono::microseconds(int64_t(_current_frame_index * _frame_duration * 1000000));
    _playback_start = now - frame_time;
  }

  _state   = State::PLAYING;
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
  _state   = State::STOPPED;
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
  auto impl = _audio_impl.getShared<AudioImpl>();
  impl->_accumulator.atomicOp([](AudioImpl::ringbuffer_ptr_t& unlocked){
    unlocked->clear();
  });
  impl->_source->_chunk_index = 0;
  lev2::audioinputchunk_ptr_t chunk;
  while(!impl->_source->_inputqueue.try_pop(chunk)){
    // drain    
  }
  impl->_source->_chunk_index = 0;
  impl->_source->_was_reset = true;
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
    _image_provider        = std::make_shared<ImageProvider>();
    _image_provider->_func = [this]() -> image_ptr_t {
      std::lock_guard<std::mutex> img_lock(_image_mutex);
      std::lock_guard<std::mutex> queue_lock(_queue_mutex);

      if (_state != State::PLAYING) {
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

      // Return current frame (repeat if queue empty)
      return _current_image;
    };
  }
  return _image_provider;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::_decodeThreadFunc() {
  AVPacket packet;
  AVFrame* frame     = av_frame_alloc();
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

          // Get channel count from codec context, NOT from the frame
          int channels = _audio_codec_ctx->ch_layout.nb_channels;
          if (channels == 0) {
            // Try codecpar as fallback
            AVCodecParameters* audio_codecpar = _format_ctx->streams[_audio_stream_idx]->codecpar;
            channels                          = audio_codecpar->ch_layout.nb_channels;
            if (channels == 0) {
              printf("WARNING: Cannot determine channel count, skipping audio frame\n");
              continue;
            }
          }

          // Create audio frame
          auto audio_frame          = std::make_shared<MovieAudioFrame>();
          audio_frame->_sample_rate = _audio_config->_sample_rate;
          audio_frame->_channels    = channels;
          audio_frame->_pts         = frame->pts * av_q2d(_format_ctx->streams[_audio_stream_idx]->time_base);

          // Convert to float samples
          audio_frame->_samples.resize(frame->nb_samples * audio_frame->_channels);

          if (frame->format == AV_SAMPLE_FMT_S16) {
            // Interleaved S16
            int16_t* samples = (int16_t*)frame->data[0];
            for (int i = 0; i < frame->nb_samples * audio_frame->_channels; i++) {
              audio_frame->_samples[i] = samples[i] / 32768.0f;
            }
          } else if (frame->format == AV_SAMPLE_FMT_FLT) {
            // Interleaved float
            float* samples = (float*)frame->data[0];
            memcpy(audio_frame->_samples.data(), samples, frame->nb_samples * audio_frame->_channels * sizeof(float));
          } else if (frame->format == AV_SAMPLE_FMT_FLTP) {
            // Planar float - CORRECT handling for 6 channels
            for (int i = 0; i < frame->nb_samples; i++) {
              for (int c = 0; c < audio_frame->_channels; c++) {
                if (frame->data[c]) { // Safety check
                  float* channel_data                                   = (float*)frame->data[c];
                  audio_frame->_samples[i * audio_frame->_channels + c] = channel_data[i];
                } else {
                  // Channel data not available, use silence
                  audio_frame->_samples[i * audio_frame->_channels + c] = 0.0f;
                }
              }
            }
          } else if (frame->format == AV_SAMPLE_FMT_S16P) {
            // Planar S16
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

audio::singularity::prgdata_ptr_t createStreamingOscillatorFromMoviePlayback(
    movieplayback_ptr_t movie_playback,            //
    audio::singularity::synth_ptr_t audio_synth) { //

  auto impl = movie_playback->_audio_impl.makeShared<AudioImpl>(movie_playback.get(),audio_synth);
  return impl->_prgdata;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
