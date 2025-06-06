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
  : _low_watermark(8192)      // ~186ms at 44.1kHz
  , _high_watermark(32768)    // ~743ms at 44.1kHz
  , _target_latency_ms(750.0f) // Target 750ms latency for network streaming
  , _interpolate_dropouts(true)
  , _adaptive_buffering(true) {
}

dspblk_ptr_t STREAMING_OSCILLATOR_DATA::createInstance() const {
  auto instance = std::make_shared<StreamingOscillatorBlock>(this);
  return instance;
}

StreamingOscillatorBlock::StreamingOscillatorBlock(const DspBlockData* dbd)
  : DspBlock(dbd)
  , _ringBuffer(4194304)  // 4MB ring buffer (~95 seconds at 44.1kHz mono float)
  , _underrun_count(0)
  , _last_sample(0.0f)
  , _fade_samples(128)
  , _is_priming(true)
  , _total_samples_processed(0)
  , _total_samples_dropped(0)
  , _playback_rate(1.0f)
  , _chunks_received_total(0)
  , _last_chunk_time(0.0) {
  _streamingdata = dynamic_cast<const STREAMING_OSCILLATOR_DATA*>(dbd);
  
  // Calculate dynamic watermarks based on sample rate when available
  float sample_rate = 48000.0f; // default
  if(synth::instance()) {
    sample_rate = synth::instance()->_sampleRate;
  }
  
  size_t samples_per_ms = size_t(sample_rate / 1000.0f);
  
  // Set watermarks based on target latency
  _dynamic_low_watermark = samples_per_ms * (_streamingdata->_target_latency_ms * 0.25f);  // 25% of target
  _dynamic_high_watermark = samples_per_ms * (_streamingdata->_target_latency_ms * 1.5f); // 150% of target
  _dynamic_target_level = samples_per_ms * _streamingdata->_target_latency_ms;
  
  // Initialize timing
  _stats_timer = std::chrono::steady_clock::now();
  _startup_time = _stats_timer;
  
  printf("STREAMING_OSC: Initialized with target latency %.1fms (%zu samples)\n", 
         _streamingdata->_target_latency_ms, _dynamic_target_level);
  printf("STREAMING_OSC: Watermarks - Low: %zu, Target: %zu, High: %zu\n",
         _dynamic_low_watermark, _dynamic_target_level, _dynamic_high_watermark);
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
  // Transfer from input chunk queue to ring buffer
  // Process ALL available chunks to minimize latency
  ///////////////////////////////
  
  size_t chunks_processed = 0;
  size_t samples_added = 0;
  lev2::audioinputchunk_ptr_t chunk;
  
  // Timing for adaptive buffering
  auto current_time = std::chrono::steady_clock::now();
  double time_since_start = std::chrono::duration<double>(current_time - _startup_time).count();
  
  while(source->_inputqueue.try_pop(chunk)) {
    auto& chan0 = chunk->_channels[0];
    size_t num_samples = chan0.size();
    const float* src = chan0.data();
    
    // Track chunk timing
    if(_last_chunk_time > 0.0) {
      double chunk_interval = time_since_start - _last_chunk_time;
      _chunk_intervals.push_back(chunk_interval);
      if(_chunk_intervals.size() > 20) {
        _chunk_intervals.pop_front();
      }
    }
    _last_chunk_time = time_since_start;
    
    // Check if ring buffer has space
    size_t available_space = _ringBuffer.capacity() - _ringBuffer.size();
    if(available_space >= num_samples) {
      _ringBuffer.push_many(src, num_samples);
      samples_added += num_samples;
      chunks_processed++;
      _chunks_received_total++;
    } else {
      // Ring buffer overflow - this shouldn't happen with proper sizing
      size_t to_push = std::min(available_space, num_samples);
      if(to_push > 0) {
        _ringBuffer.push_many(src, to_push);
        samples_added += to_push;
      }
      _total_samples_dropped += (num_samples - to_push);
      
      // Push chunk back for next iteration
      source->_inputqueue.push(chunk);
      break;
    }
  }

  ///////////////////////////////
  // Adaptive buffer management
  ///////////////////////////////
  
  size_t current_buffer_size = _ringBuffer.size();
  
  // Calculate average chunk interval
  double avg_chunk_interval = 0.0;
  if(!_chunk_intervals.empty()) {
    double sum = 0.0;
    for(double interval : _chunk_intervals) {
      sum += interval;
    }
    avg_chunk_interval = sum / _chunk_intervals.size();
  }
  
  // Adaptive playback rate adjustment
  if(_streamingdata->_adaptive_buffering && !_is_priming) {
    float buffer_ratio = float(current_buffer_size) / float(_dynamic_target_level);
    
    // Adjust playback rate to maintain target buffer level
    if(buffer_ratio < 0.5f) {
      // Buffer too low - slow down playback slightly
      _playback_rate = 0.98f;
    } else if(buffer_ratio > 1.5f) {
      // Buffer too high - speed up playback slightly
      _playback_rate = 1.02f;
    } else {
      // Buffer in good range - normal playback
      _playback_rate = 1.0f;
    }
    
    // Smooth rate changes
    static float smooth_rate = 1.0f;
    smooth_rate = smooth_rate * 0.99f + _playback_rate * 0.01f;
    _playback_rate = smooth_rate;
  }
  
  // Extended priming phase for network streaming
  if(_is_priming) {
    bool have_enough_chunks = _chunks_received_total >= 10; // Wait for at least 10 chunks
    bool buffer_filled = current_buffer_size >= _dynamic_target_level;
    bool time_elapsed = time_since_start > 2.0; // Max 2 seconds priming
    
    if((have_enough_chunks && buffer_filled) || time_elapsed) {
      _is_priming = false;
      printf("STREAMING_OSC: Priming complete - chunks:%zu buffer:%zu time:%.2fs\n",
             _chunks_received_total.load(), current_buffer_size, time_since_start);
    } else {
      // Output silence while priming
      memset(outputchan, 0, inumframes * sizeof(float));
      return;
    }
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
    printf("STREAMING_OSC: UNDERRUN #%zu - buffer:%zu needed:%d\n", 
           _underrun_count.load(), current_buffer_size, inumframes);
  }
  
  _was_underrun = is_underrun;

  ///////////////////////////////
  // Generate output with resampling
  ///////////////////////////////
  
  int samples_to_consume = int(inumframes * _playback_rate);
  
  if(is_underrun) {
    // Complete underrun - output silence
    memset(outputchan, 0, inumframes * sizeof(float));
    _last_sample = 0.0f;
  } else if(is_low_buffer && _streamingdata->_interpolate_dropouts) {
    // Low buffer - reduce consumption and interpolate
    size_t samples_available = std::min(current_buffer_size, size_t(inumframes / 2));
    
    if(samples_available > 0) {
      // Create temp buffer for resampling
      std::vector<float> temp_buffer(samples_available);
      _ringBuffer.pop_many(temp_buffer.data(), samples_available);
      
      // Simple linear interpolation to stretch audio
      for(int i = 0; i < inumframes; i++) {
        float pos = float(i) * float(samples_available - 1) / float(inumframes - 1);
        int idx0 = int(pos);
        int idx1 = std::min(idx0 + 1, int(samples_available - 1));
        float frac = pos - float(idx0);
        
        outputchan[i] = temp_buffer[idx0] * (1.0f - frac) + temp_buffer[idx1] * frac;
        
        // Apply fade based on buffer level
        float fade = float(current_buffer_size) / float(_dynamic_low_watermark);
        fade = std::max(0.2f, std::min(1.0f, fade)); // Keep at least 20% volume
        outputchan[i] *= fade;
      }
      
      _last_sample = outputchan[inumframes - 1];
    } else {
      // No samples available
      memset(outputchan, 0, inumframes * sizeof(float));
      _last_sample = 0.0f;
    }
  } else if(current_buffer_size >= samples_to_consume) {
    // Normal operation with resampling
    if(fabs(_playback_rate - 1.0f) < 0.001f) {
      // No resampling needed
      _ringBuffer.pop_many(outputchan, inumframes);
    } else {
      // Resample
      std::vector<float> temp_buffer(samples_to_consume);
      _ringBuffer.pop_many(temp_buffer.data(), samples_to_consume);
      
      // Linear interpolation resampling
      for(int i = 0; i < inumframes; i++) {
        float pos = float(i) * float(samples_to_consume - 1) / float(inumframes - 1);
        int idx0 = int(pos);
        int idx1 = std::min(idx0 + 1, samples_to_consume - 1);
        float frac = pos - float(idx0);
        
        outputchan[i] = temp_buffer[idx0] * (1.0f - frac) + temp_buffer[idx1] * frac;
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
  } else {
    // Partial underrun - get what we can
    size_t samples_available = current_buffer_size;
    if(samples_available > 0) {
      _ringBuffer.pop_many(outputchan, samples_available);
      _last_sample = outputchan[samples_available - 1];
    }
    
    // Zero-fill the rest
    memset(outputchan + samples_available, 0, (inumframes - samples_available) * sizeof(float));
  }

  _total_samples_processed += inumframes;

  ///////////////////////////////
  // Periodic statistics logging
  ///////////////////////////////
  
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - _stats_timer).count();
  
  if(elapsed >= 5) {  // Log every 5 seconds
    float buffer_fullness = float(current_buffer_size) / float(_ringBuffer.capacity()) * 100.0f;
    float target_fullness = float(current_buffer_size) / float(_dynamic_target_level) * 100.0f;
    float sample_rate = synth::instance() ? synth::instance()->_sampleRate : 48000.0f;
    
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
}

void StreamingOscillatorBlock::doKeyOff() {
  // Could implement fade-out here if needed
}

} // namespace ork::audio::singularity