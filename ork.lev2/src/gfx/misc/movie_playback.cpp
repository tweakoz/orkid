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
// Backend Dispatching Implementation
///////////////////////////////////////////////////////////////////////////////////////////////

// Legacy init (defaults to FFmpeg backend for 100% compatibility)
void MoviePlaybackContext::init(const std::string& filename) {
  init(filename, MovieBackend::FFMPEG, MoviePixelFormat::AUTO);
}

// New init with backend selection
void MoviePlaybackContext::init(const std::string& filename,
                                 MovieBackend backend,
                                 MoviePixelFormat format) {
  _filename = filename;
  _backend = backend;
  _pixel_format = format;

  // Create appropriate backend
  switch (_backend) {
    case MovieBackend::FFMPEG:
      _backend_impl = createFFmpegBackend(this);
      break;

#if defined(__APPLE__)
    case MovieBackend::VIDEOTOOLBOX:
      _backend_impl = createVideoToolboxBackend(this);
      break;
#endif

#if defined(__linux__)
    case MovieBackend::VAAPI:
      _backend_impl = createVAAPIBackend(this);
      break;
    case MovieBackend::NVDEC:
      _backend_impl = createNVDECBackend(this);
      break;
#endif

    default:
      printf("ERROR: Unsupported movie backend %d for this platform\n", (int)_backend);
      // Fallback to FFmpeg
      _backend = MovieBackend::FFMPEG;
      _backend_impl = createFFmpegBackend(this);
      break;
  }

  // Initialize backend
  if (_backend_impl) {
    bool success = _backend_impl->init(filename, format);
    if (!success) {
      printf("ERROR: Failed to initialize movie backend\n");
      _backend_impl.reset();
    } else {
      // Populate context members from backend for backward compatibility
      _fps = _backend_impl->fps();
      _duration = _backend_impl->duration();
      _video_width = _backend_impl->width();
      _video_height = _backend_impl->height();
      _audio_config = _backend_impl->audioConfig();
      _frame_duration = 1.0 / _fps;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::play() {
  if (_backend_impl) {
    _backend_impl->play();
  }
}

void MoviePlaybackContext::pause() {
  if (_backend_impl) {
    _backend_impl->pause();
  }
}

void MoviePlaybackContext::stop() {
  if (_backend_impl) {
    _backend_impl->stop();
  }
}

void MoviePlaybackContext::restart() {
  // Handle audio cleanup before backend restart
  auto impl = _audio_impl.getShared<AudioImpl>();
  if (impl) {
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
  }

  // Restart backend
  if (_backend_impl) {
    _backend_impl->restart();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::setAudioCallback(audio_callback_t cb) {
  if (_backend_impl) {
    _backend_impl->setAudioCallback(cb);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////

image_provider_ptr_t MoviePlaybackContext::createImageProvider() {
  if (_backend_impl) {
    return _backend_impl->createImageProvider();
  }
  return nullptr;
}

texture_provider_ptr_t MoviePlaybackContext::createTextureProvider() {
  if (_backend_impl) {
    return _backend_impl->createTextureProvider();
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void MoviePlaybackContext::_decodeThreadFunc() {
  // This is now handled by individual backends
  // Kept here for compatibility (called by old code paths)
}

void MoviePlaybackContext::_cleanup() {
  // Backend cleanup is handled by backend destructor
  _backend_impl.reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////

audio::singularity::prgdata_ptr_t createStreamingOscillatorFromMoviePlayback(
    movieplayback_ptr_t movie_playback,
    audio::singularity::synth_ptr_t audio_synth) {
  auto impl = movie_playback->_audio_impl.makeShared<AudioImpl>(movie_playback.get(),audio_synth);
  return impl->_prgdata;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

