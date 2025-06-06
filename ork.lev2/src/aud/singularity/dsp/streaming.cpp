////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <sndfile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <chrono>
#include <atomic>
#include <deque>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::STREAMING_OSCILLATOR_DATA, "DspStreamingOscillator");

namespace ork::audio::singularity {

void STREAMING_OSCILLATOR_DATA::describeX(class_t* clazz) {
  // Add configurable parameters
  clazz->directProperty("LowWatermark", &STREAMING_OSCILLATOR_DATA::_low_watermark);
  clazz->directProperty("HighWatermark", &STREAMING_OSCILLATOR_DATA::_high_watermark);
  clazz->directProperty("TargetLatencyMs", &STREAMING_OSCILLATOR_DATA::_target_latency_ms);
  clazz->directProperty("InterpolateDropouts", &STREAMING_OSCILLATOR_DATA::_interpolate_dropouts);
  clazz->directProperty("AdaptiveBuffering", &STREAMING_OSCILLATOR_DATA::_adaptive_buffering);
}

STREAMING_OSCILLATOR_DATA::STREAMING_OSCILLATOR_DATA(std::string name)
  : _low_watermark(24000)      // ~500ms at 48kHz
  , _high_watermark(36000)     // ~750ms at 48kHz
  , _target_latency_ms(750.0f) // Target 750ms latency for network streaming
  , _interpolate_dropouts(false)
  , _adaptive_buffering(true) {
}

dspblk_ptr_t STREAMING_OSCILLATOR_DATA::createInstance() const {
  auto instance = std::make_shared<StreamingOscillatorBlock>(this);
  return instance;
}

StreamingOscillatorBlock::StreamingOscillatorBlock(const DspBlockData* dbd)
  : DspBlock(dbd)
  , _ringBuffer(96000)  // 2 seconds at 48kHz 
  , _dynamic_low_watermark(0)
  , _dynamic_high_watermark(0)
  , _dynamic_target_level(0)
  , _underrun_count(0)
  , _total_samples_processed(0)
  , _total_samples_dropped(0)
  , _last_sample(0.0f)
  , _fade_samples(128)
  , _is_priming(true)
  , _was_underrun(false)
  , _playback_rate(1.0f)
  , _chunks_received_total(0)
  , _last_chunk_time(0.0)
  , _accumulator_buffer(1024*1024) { // 1MB accumulator
  
  _streamingdata = dynamic_cast<const STREAMING_OSCILLATOR_DATA*>(dbd);
  
  // Calculate dynamic watermarks based on sample rate when available
  float sample_rate = 48000.0f; // default
  if(synth::instance()) {
    sample_rate = synth::instance()->_sampleRate;
  }
  
  size_t samples_per_ms = size_t(sample_rate / 1000.0f);
  
  // Use configured watermarks or calculate from target latency
  if(_streamingdata->_low_watermark > 0) {
    _dynamic_low_watermark = _streamingdata->_low_watermark;
  } else {
    _dynamic_low_watermark = samples_per_ms * (_streamingdata->_target_latency_ms * 0.5f);
  }
  
  if(_streamingdata->_high_watermark > 0) {
    _dynamic_high_watermark = _streamingdata->_high_watermark;
  } else {
    _dynamic_high_watermark = samples_per_ms * (_streamingdata->_target_latency_ms * 1.5f);
  }
  
  _dynamic_target_level = samples_per_ms * _streamingdata->_target_latency_ms;
  
  // Initialize timing
  _stats_timer = std::chrono::steady_clock::now();
  _startup_time = _stats_timer;
  
  printf("STREAMING_OSC: Initialized with target latency %.1fms (%zu samples)\n", 
         _streamingdata->_target_latency_ms, _dynamic_target_level);
  printf("STREAMING_OSC: Watermarks - Low: %zu, Target: %zu, High: %zu\n",
         _dynamic_low_watermark, _dynamic_target_level, _dynamic_high_watermark);
  printf("STREAMING_OSC: Ring buffer capacity: %zu samples (%.1f seconds)\n",
         _ringBuffer.capacity(), float(_ringBuffer.capacity()) / sample_rate);
}

void StreamingOscillatorBlock::compute(DspBuffer& dspbuf) {
  auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  int inumframes = _layer->_dspwritecount;
  
  auto source = _streamingdata->_source;
  if(!source) {
    // No source, output silence
    memset(outputchan, 0, inumframes * sizeof(float));
    return;
  }

  ///////////////////////////////
  // Accumulate all available chunks before processing
  ///////////////////////////////
  
  size_t chunks_processed = 0;
  size_t samples_added = 0;
  size_t total_samples_available = 0;
  lev2::audioinputchunk_ptr_t chunk;
  
  // First, accumulate all available chunks into a temporary buffer
  _accumulator_buffer.clear();
  
  while(source->_inputqueue.try_pop(chunk)) {
    auto& chan0 = chunk->_channels[0];
    size_t num_samples = chan0.size();
    const float* src = chan0.data();
    
    // Add to accumulator
    _accumulator_buffer.insert(_accumulator_buffer.end(), src, src + num_samples);
    total_samples_available += num_samples;
    chunks_processed++;
    _chunks_received_total++;
    
    // Track timing
    auto current_time = std::chrono::steady_clock::now();
    double time_since_start = std::chrono::duration<double>(current_time - _startup_time).count();
    
    if(_last_chunk_time > 0.0) {
      double chunk_interval = time_since_start - _last_chunk_time;
      _chunk_intervals.push_back(chunk_interval);
      if(_chunk_intervals.size() > 20) {
        _chunk_intervals.pop_front();
      }
    }
    _last_chunk_time = time_since_start;
  }

  ///////////////////////////////
  // Transfer accumulated samples to ring buffer
  ///////////////////////////////
  
  if(!_accumulator_buffer.empty()) {
    size_t available_space = _ringBuffer.capacity() - _ringBuffer.size();
    size_t to_push = std::min(available_space, _accumulator_buffer.size());
    
    if(to_push > 0) {
      _ringBuffer.push_many(_accumulator_buffer.data(), to_push);
      samples_added = to_push;
    }
    
    if(to_push < _accumulator_buffer.size()) {
      _total_samples_dropped += (_accumulator_buffer.size() - to_push);
      printf("STREAMING_OSC: WARNING - Ring buffer overflow, dropped %zu samples\n", 
             _accumulator_buffer.size() - to_push);
    }
  }

  ///////////////////////////////
  // Buffer state analysis
  ///////////////////////////////
  
  size_t current_buffer_size = _ringBuffer.size();
  
  // Calculate fill rate
  static size_t last_buffer_size = 0;
  static auto last_fill_check = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  double fill_check_interval = std::chrono::duration<double>(now - last_fill_check).count();
  
  if(fill_check_interval > 0.1) { // Check every 100ms
    float sample_rate = synth::instance() ? synth::instance()->_sampleRate : 48000.0f;
    float fill_rate = float(current_buffer_size - last_buffer_size) / (fill_check_interval * sample_rate);
    float drain_rate = float(inumframes) / sample_rate;
    
    if(fill_rate < drain_rate * 0.8f && !_is_priming) {
      printf("STREAMING_OSC: WARNING - Fill rate (%.2f) < Drain rate (%.2f)\n", fill_rate, drain_rate);
    }
    
    last_buffer_size = current_buffer_size;
    last_fill_check = now;
  }
  
  // Extended priming phase for network streaming
  if(_is_priming) {
    bool have_enough_chunks = _chunks_received_total >= 20; // Wait for more chunks
    bool buffer_filled = current_buffer_size >= _dynamic_target_level;
    auto time_since_start = std::chrono::duration<double>(now - _startup_time).count();
    bool time_elapsed = time_since_start > 3.0; // Max 3 seconds priming
    
    if((have_enough_chunks && buffer_filled) || time_elapsed) {
      _is_priming = false;
      printf("STREAMING_OSC: Priming complete - chunks:%zu buffer:%zu time:%.2fs\n",
             _chunks_received_total.load(), current_buffer_size, time_since_start);
    } else {
      // Output silence while priming
      memset(outputchan, 0, inumframes * sizeof(float));
      
      // Periodic priming status
      static auto last_prime_log = std::chrono::steady_clock::now();
      if(std::chrono::duration<double>(now - last_prime_log).count() > 0.5) {
        printf("STREAMING_OSC: Priming... chunks:%zu buffer:%zu (%.1f%% of target)\n",
               _chunks_received_total.load(), current_buffer_size,
               float(current_buffer_size) / float(_dynamic_target_level) * 100.0f);
        last_prime_log = now;
      }
      return;
    }
  }

  ///////////////////////////////
  // Adaptive playback rate adjustment
  ///////////////////////////////
  
  if(_streamingdata->_adaptive_buffering && !_is_priming) {
    float buffer_ratio = float(current_buffer_size) / float(_dynamic_target_level);
    
    // More aggressive rate adjustment for severe under/overfill
    if(current_buffer_size < _dynamic_low_watermark) {
      // Critical - slow down significantly
      _playback_rate = 0.95f;
    } else if(buffer_ratio < 0.7f) {
      // Low - slow down moderately
      _playback_rate = 0.98f;
    } else if(buffer_ratio > 1.3f) {
      // High - speed up moderately
      _playback_rate = 1.02f;
    } else if(current_buffer_size > _dynamic_high_watermark) {
      // Critical - speed up significantly
      _playback_rate = 1.05f;
    } else {
      // Good range - normal playback
      _playback_rate = 1.0f;
    }
    
    // Smooth rate changes with faster response
    static float smooth_rate = 1.0f;
    smooth_rate = smooth_rate * 0.95f + _playback_rate * 0.05f;
    _playback_rate = smooth_rate;
  }

  ///////////////////////////////
  // Check for underrun condition
  ///////////////////////////////
  
  bool is_underrun = (current_buffer_size < size_t(inumframes));
  bool is_low_buffer = (current_buffer_size < _dynamic_low_watermark);
  
  if(is_underrun && !_was_underrun) {
    // Just entered underrun state
    _underrun_count++;
    _underrun_start_time = std::chrono::steady_clock::now();
    printf("STREAMING_OSC: UNDERRUN #%zu - buffer:%zu needed:%d (%.1f%% of target)\n", 
           _underrun_count.load(), current_buffer_size, inumframes,
           float(current_buffer_size) / float(_dynamic_target_level) * 100.0f);
  }
  
  _was_underrun = is_underrun;

  ///////////////////////////////
  // Generate output
  ///////////////////////////////
  
  if(is_underrun) {
    // Complete underrun - output silence
    memset(outputchan, 0, inumframes * sizeof(float));
    _last_sample = 0.0f;
  } else {
    // We have enough samples - apply playback rate if needed
    int samples_to_consume = int(inumframes * _playback_rate);
    samples_to_consume = std::min(samples_to_consume, int(current_buffer_size));
    
    if(fabs(_playback_rate - 1.0f) < 0.001f || !_streamingdata->_adaptive_buffering) {
      // Normal playback - no resampling
      _ringBuffer.pop_many(outputchan, inumframes);
    } else {
      // Resample with playback rate adjustment
      if(samples_to_consume > 0) {
        std::vector<float> temp_buffer(samples_to_consume);
        _ringBuffer.pop_many(temp_buffer.data(), samples_to_consume);
        
        // Simple linear interpolation resampling
        for(int i = 0; i < inumframes; i++) {
          float pos = float(i) * float(samples_to_consume - 1) / float(inumframes - 1);
          int idx0 = int(pos);
          int idx1 = std::min(idx0 + 1, samples_to_consume - 1);
          float frac = pos - float(idx0);
          
          outputchan[i] = temp_buffer[idx0] * (1.0f - frac) + temp_buffer[idx1] * frac;
        }
      } else {
        memset(outputchan, 0, inumframes * sizeof(float));
      }
    }
    
    _last_sample = outputchan[inumframes - 1];
    
    // Apply micro-fade if recovering from underrun
    if(_was_underrun && !is_underrun) {
      int fade_length = std::min(_fade_samples, inumframes);
      for(int i = 0; i < fade_length; i++) {
        float t = float(i) / float(fade_length);
        outputchan[i] *= t;
      }
    }
  }

  _total_samples_processed += inumframes;

  ///////////////////////////////
  // Periodic statistics logging
  ///////////////////////////////
  
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - _stats_timer).count();
  
  if(elapsed >= 5) {  // Log every 5 seconds
    float buffer_fullness = float(current_buffer_size) / float(_ringBuffer.capacity()) * 100.0f;
    float target_fullness = float(current_buffer_size) / float(_dynamic_target_level) * 100.0f;
    float sample_rate = synth::instance() ? synth::instance()->_sampleRate : 48000.0f;
    
    // Calculate average chunk interval
    double avg_chunk_interval = 0.0;
    if(!_chunk_intervals.empty()) {
      double sum = 0.0;
      for(double interval : _chunk_intervals) {
        sum += interval;
      }
      avg_chunk_interval = sum / _chunk_intervals.size();
    }
    
    printf("STREAMING_OSC: buffer:%zu/%zu (%.1f%% full, %.1f%% of target) "
           "underruns:%zu dropped:%zu chunks_in:%zu latency:%.1fms target:%.1fms "
           "rate:%.3f avg_interval:%.3fs\n",
           current_buffer_size, _ringBuffer.capacity(), buffer_fullness, target_fullness,
           _underrun_count.load(), _total_samples_dropped.load(), 
           chunks_processed,
           float(current_buffer_size) / (sample_rate / 1000.0f),
           _streamingdata->_target_latency_ms,
           _playback_rate,
           avg_chunk_interval);
    
    _stats_timer = now;
  }
}

void StreamingOscillatorBlock::doKeyOn(const KeyOnInfo& koi) {
  // Reset state on key-on
  _is_priming = true;
  _underrun_count = 0;
  _last_sample = 0.0f;
  _total_samples_processed = 0;
  _total_samples_dropped = 0;
  _chunks_received_total = 0;
  _playback_rate = 1.0f;
  _chunk_intervals.clear();
  _last_chunk_time = 0.0;
  _startup_time = std::chrono::steady_clock::now();
  _accumulator_buffer.clear();
  
  // Clear ring buffer
  float dummy;
  while(_ringBuffer.try_pop(dummy)) {}
  
  printf("STREAMING_OSC: KeyOn - starting priming phase\n");
}

void StreamingOscillatorBlock::doKeyOff() {
  // Could implement fade-out here if needed
  printf("STREAMING_OSC: KeyOff - processed %zu samples total\n", _total_samples_processed.load());
}

} // namespace ork::audio::singularity