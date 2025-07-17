////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/aud/singularity/fft.h>
#include <dspstretch/signalsmith-stretch.h>

namespace ork::audio::singularity {

static logchannel_ptr_t logchan_strcimpl = logger()->getChannel("PERF");

////////////////////////////////////////////////////////////////

struct UBTS {
  using stretcher_t = signalsmith::stretch::SignalsmithStretch<float>;

  std::shared_ptr<stretcher_t> _stretcher;
  std::vector<float> _input_buffer;
  std::vector<float> _output_buffer;
  bool _initialized = false;
  float _smoothed_ratio;
  float _alpha;

  //////////////////////////////

  UBTS() //
      : _smoothed_ratio(1.0f)
      , //
      _alpha(0.99f) {
    // Much more aggressive smoothing
  }

  //////////////////////////////

  void initialize(float sample_rate) {
    if (_initialized)
      return;

    _stretcher = std::make_shared<stretcher_t>();
    _stretcher->presetDefault(1, sample_rate); // 1 channel

    int block_size = _stretcher->blockSamples();
    _input_buffer.resize(block_size);
    _output_buffer.resize(block_size);

    _initialized = true;
    logchan_strcimpl->log("UBTS Initialized with block_size=%d", block_size);
  }

  //////////////////////////////

  void process(const float* input, float* output, int input_samples, int output_samples, float target_ratio) {
    // Clamp target ratio to safe bounds before smoothing
    target_ratio = std::max(0.1f, std::min(target_ratio, 10.0f));

    // Smooth ratio changes to avoid clicks
    _smoothed_ratio = _alpha * _smoothed_ratio + (1.0f - _alpha) * target_ratio;

    // Clamp smoothed ratio to safe bounds
    _smoothed_ratio = std::max(0.1f, std::min(_smoothed_ratio, 10.0f));

    // Bypass if ratio is very close to 1.0
    if (fabsf(_smoothed_ratio - 1.0f) < 0.001f) {
      int copy_samples = std::min(input_samples, output_samples);
      memcpy(output, input, copy_samples * sizeof(float));
      if (output_samples > copy_samples) {
        memset(output + copy_samples, 0, (output_samples - copy_samples) * sizeof(float));
      }
      return;
    }

    if (!_initialized) {
      // Fallback: copy what we can
      int copy_samples = std::min(input_samples, output_samples);
      memcpy(output, input, copy_samples * sizeof(float));
      if (output_samples > copy_samples) {
        memset(output + copy_samples, 0, (output_samples - copy_samples) * sizeof(float));
      }
      return;
    }

    // For now, use the original signalsmith approach but adapt for variable ratios
    // The challenge is that signalsmith expects fixed block sizes

    // Set transpose to 0 for pure time stretching (no pitch change)
    _stretcher->setTransposeSemitones(0.0f);

    // Simple block processing - similar to original but with ratio handling
    int block_size = _stretcher->blockSamples();
    if (input_samples <= block_size && output_samples <= block_size) {
      // Copy input to buffer
      memcpy(_input_buffer.data(), input, input_samples * sizeof(float));

      // Pad with zeros if needed
      if (input_samples < block_size) {
        memset(_input_buffer.data() + input_samples, 0, (block_size - input_samples) * sizeof(float));
      }

      // Process through signalsmith
      const float* input_buffers[1] = {_input_buffer.data()};
      float* output_buffers[1]      = {_output_buffer.data()};

      _stretcher->process(input_buffers, block_size, output_buffers, block_size);

      // For time stretching, we need to resample the output from signalsmith
      // This is a hybrid approach: signalsmith for spectral quality + resampling for time ratio
      if (fabsf(_smoothed_ratio - 1.0f) > 0.001f) {
        // Apply time stretching via resampling the signalsmith output
        for (int i = 0; i < output_samples; i++) {
          float pos = float(i) * _smoothed_ratio;
          int idx   = int(pos);
          if (idx >= block_size - 1) {
            output[i] = 0.0f;
            continue;
          }
          float frac = pos - float(idx);
          output[i]  = _output_buffer[idx] + frac * (_output_buffer[idx + 1] - _output_buffer[idx]);
        }
      } else {
        // No stretching needed - direct copy
        memcpy(output, _output_buffer.data(), output_samples * sizeof(float));
      }
    } else {
      // Fallback: copy what we can
      int copy_samples = std::min(input_samples, output_samples);
      memcpy(output, input, copy_samples * sizeof(float));
      if (output_samples > copy_samples) {
        memset(output + copy_samples, 0, (output_samples - copy_samples) * sizeof(float));
      }
    }
  }
  //////////////////////////////
};

////////////////////////////////////////////////////////////////

struct BufferedTimeStretcher {

  //////////////////////////////

  UBTS _resampler;
  ork::RingBuffer<float> _timestretched_buffer;
  std::vector<float> _processing_buffer;
  std::vector<float> _output_buffer;

  static constexpr int PROCESS_CHUNK_SIZE = 1024; // Process in 1K chunks for effective timestretching
  static constexpr int OUTPUT_BUFFER_SIZE = 4096; // 4K buffer for timestretched output

  bool _initialized = false;

  //////////////////////////////

  BufferedTimeStretcher()
      : _timestretched_buffer(OUTPUT_BUFFER_SIZE) {
    _processing_buffer.resize(PROCESS_CHUNK_SIZE);
    _output_buffer.resize(PROCESS_CHUNK_SIZE * 2); // Allow for up to 2x expansion
  }

  //////////////////////////////

  void initialize(float sample_rate) {
    if (_initialized)
      return;
    _resampler.initialize(sample_rate);
    _initialized = true;
    logchan_strcimpl->log(
        "BufferedTimeStretcher: Initialized with %dK processing chunks, %dK output buffer", PROCESS_CHUNK_SIZE, OUTPUT_BUFFER_SIZE);
  }

  //////////////////////////////
  // Process larger chunks when available, with flow control to prevent overflow
  //////////////////////////////
  void processAvailableData(ork::RingBuffer<float>& input_buffer, float stretch_ratio) {
    if (!_initialized) {
      float sample_rate = synth::instance() ? synth::instance()->_sampleRate : 48000.0f;
      initialize(sample_rate);
    }

    // CRITICAL: Only process if we have room in the output buffer
    // Keep at least 1024 samples of free space to prevent overflow
    size_t min_free_space  = 1024;
    size_t available_space = _timestretched_buffer.capacity() - _timestretched_buffer.size();

    if (available_space < min_free_space) {
      // Output buffer is too full - skip processing this cycle
      return;
    }

    // Only process when we have enough input data AND enough output space
    if (input_buffer.size() >= PROCESS_CHUNK_SIZE) {
      // Pull a processing chunk
      input_buffer.pop_many(_processing_buffer.data(), PROCESS_CHUNK_SIZE);

      // Calculate output size based on stretch ratio
      // stretch_ratio < 1.0 = slow down = more output samples
      // stretch_ratio > 1.0 = speed up = fewer output samples
      int output_samples = int(float(PROCESS_CHUNK_SIZE) / stretch_ratio);
      output_samples     = std::max(1, std::min(output_samples, PROCESS_CHUNK_SIZE * 2)); // Allow up to 2x expansion

      // Make sure we don't exceed available space OR output buffer size
      output_samples = std::min(output_samples, int(available_space));
      output_samples = std::min(output_samples, int(_output_buffer.size()));

      // Apply variable rate resampling
      _resampler.process(_processing_buffer.data(), _output_buffer.data(), PROCESS_CHUNK_SIZE, output_samples, stretch_ratio);

      // Push stretched data to output buffer
      _timestretched_buffer.push_many(_output_buffer.data(), output_samples);

      // Only log occasionally to avoid spam
      static int process_count = 0;
      if (++process_count % 10 == 0) {
        logchan_strcimpl->perfItem("SIMPL:BTS:output_samples", int(output_samples));
        logchan_strcimpl->perfItem("SIMPL:BTS:proc_count", int(process_count));
      }
    }
  }

  //////////////////////////////
  // Get samples for fixed-frame output
  //////////////////////////////

  bool getSamples(float* output, int frames) {
    if (_timestretched_buffer.size() >= size_t(frames)) {
      _timestretched_buffer.pop_many(output, frames);
      return true;
    }
    return false;
  }

  //////////////////////////////
  size_t getAvailableSamples() const {
    return _timestretched_buffer.size();
  }

  //////////////////////////////
  void reset() {
    //_timestretched_buffer.clear();
    _resampler = UBTS();
  }
};

////////////////////////////////////////////////////////////////

struct ComplexImpl {
  StreamingOscillatorBlock* _oscil;
  bool _is_primed = false;
  BufferedTimeStretcher _buffered_stretcher;
  bool _use_stretching = true;

  // Fallback state for smooth transitions
  bool _was_using_stretched             = false;
  static constexpr int CROSSFADE_LENGTH = 64; // 64 samples = ~1.3ms fade

  ComplexImpl(StreamingOscillatorBlock* osc)
      : _oscil(osc) {
  }

  void reset() {
    _is_primed = false;
    _buffered_stretcher.reset();
  }

  // Emergency buffer drain when severely overfull
  void emergencyDrain() {
    size_t current_size = _oscil->_ringBuffer.size();
    size_t target_size  = _oscil->_dynamic_target_level;

    if (current_size > target_size * 1.5) { // More aggressive trigger
      size_t excess   = current_size - target_size;
      size_t to_drain = std::min(excess, size_t(16384)); // Drain max 16K samples at once

      std::vector<float> temp_drain(to_drain);
      _oscil->_ringBuffer.pop_many(temp_drain.data(), to_drain);

      logchan_strcimpl->log(
          "EMERGENCY DRAIN: Removed %zu samples, buffer now %zu (target %zu)", to_drain, _oscil->_ringBuffer.size(), target_size);
      logchan_strcimpl->perfItem("SIMPL:DrainedSamples", int(to_drain));
    }
  }

  void processInput() {
    auto source = _oscil->_streamingdata->_source;
    if (!source)
      return;

    // Process all available chunks
    lev2::audioinputchunk_ptr_t chunk;
    while (source->_inputqueue.try_pop(chunk)) {
      auto& chan0        = chunk->_channels[0];
      size_t num_samples = chan0.size();
      const float* src   = chan0.data();

      // Push to ring buffer
      size_t available_space = _oscil->_ringBuffer.capacity() - _oscil->_ringBuffer.size();
      size_t to_push         = std::min(available_space, num_samples);

      if (to_push > 0) {
        _oscil->_ringBuffer.push_many(src, to_push);
      }

      if (to_push < num_samples) {
        logchan_strcimpl->log("ComplexImpl: Dropped %zu samples", num_samples - to_push);
      }
    }
  }

  void generateOutput(DspBuffer& dspbuf) {
    auto outputchan            = _oscil->getOutBuf(dspbuf, 0) + _oscil->_layer->_dspwritebase;
    int frames                 = _oscil->_layer->_dspwritecount; // Always 64 - immutable
    size_t current_buffer_size = _oscil->_ringBuffer.size();

    ///////////////////////////////////////////////////////////////////
    // Use actual sample rate and consistent thresholds with hysteresis
    ///////////////////////////////////////////////////////////////////

    float sample_rate      = synth::instance() ? synth::instance()->_sampleRate : 48000.0f;
    size_t target_level    = _oscil->_dynamic_target_level; // Use configured target
    size_t exit_threshold  = target_level;                  // Exit priming at target level
    size_t enter_threshold = (target_level * 25) / 100;     // Re-enter priming at 25% of target

    // Emergency drain if buffer is severely overfull
    emergencyDrain();

    // Update buffer size after potential drain
    current_buffer_size = _oscil->_ringBuffer.size();

    ///////////////////////////////////////////////////////////////////
    // State transitions with hysteresis
    ///////////////////////////////////////////////////////////////////

    if (!_is_primed && current_buffer_size >= exit_threshold) {
      _is_primed = true;
      logchan_strcimpl->log("ComplexImpl: Priming complete - buffer:%zu target:%zu", current_buffer_size, target_level);
    } else if (_is_primed && current_buffer_size < enter_threshold) {
      _is_primed = false;
      logchan_strcimpl->log(
          "ComplexImpl: Re-entering priming - buffer:%zu (%.1f%% of target)",
          current_buffer_size,
          float(current_buffer_size) / float(target_level) * 100.0f);
    }

    bool should_consume_samples = (current_buffer_size > enter_threshold) and _is_primed;

    float buffer_health = float(current_buffer_size) / float(target_level);

    // Proportional control with DEAD ZONE to prevent modulation artifacts
    float deviation = buffer_health - 1.0f;

    // Add dead zone around target level - no stretching within ±5% of target
    static constexpr float DEAD_ZONE = 0.05f; // 5% dead zone
    if (fabsf(deviation) < DEAD_ZONE) {
      deviation = 0.0f; // Force to exact target within dead zone
    } else {
      // Reduce effective deviation by dead zone amount
      if (deviation > 0) {
        deviation -= DEAD_ZONE;
      } else {
        deviation += DEAD_ZONE;
      }
    }

    // Much gentler control gain to reduce audible modulation
    float gain = 0.05f; // Reduced from 0.2f for much gentler control
    if (fabsf(deviation) > 0.5f) {
      gain = 0.1f; // Reduced from 0.5f
    }

    float stretch_ratio = 1.0f - (gain * deviation); // Negative feedback

    // Clamp to safe bounds (tighter range to reduce artifacts)
    stretch_ratio = std::max(0.98f, std::min(1.02f, stretch_ratio));

    ///////////////////////////////////////////////////////////////////
    // BUFFERED TIMESTRETCHING PROCESSING
    ///////////////////////////////////////////////////////////////////

    if (_use_stretching && should_consume_samples) {
      // Only process when timestretched buffer is running low to prevent overflow
      size_t stretched_buffer_size = _buffered_stretcher.getAvailableSamples();

      // Add hysteresis to prevent rapid on/off processing cycles
      static bool was_processing        = false;
      size_t start_processing_threshold = 1024; // Start processing when buffer drops below 1024
      size_t stop_processing_threshold  = 2048; // Stop processing when buffer reaches 2048

      bool should_process = false;
      if (!was_processing && stretched_buffer_size < start_processing_threshold) {
        should_process = true;
        was_processing = true;
      } else if (was_processing && stretched_buffer_size < stop_processing_threshold) {
        should_process = true;
        // Keep processing until we reach stop threshold
      } else if (was_processing && stretched_buffer_size >= stop_processing_threshold) {
        was_processing = false;
        should_process = false;
      }

      if (should_process) {
        // Process available data in larger chunks for effective timestretching
        _buffered_stretcher.processAvailableData(_oscil->_ringBuffer, stretch_ratio);
      }
    }

    ///////////////////////////////////////////////////////////////////
    // logging
    ///////////////////////////////////////////////////////////////////

    if (1) {
      logchan_strcimpl->perfItem("SIMPL:BufferCur", int(current_buffer_size));
      logchan_strcimpl->perfItem("SIMPL:BufferTgt", int(target_level));
      logchan_strcimpl->perfItem("SIMPL:BufferHealth", buffer_health);
      logchan_strcimpl->perfItem("SIMPL:StretchRatio", stretch_ratio);
      logchan_strcimpl->perfItem("SIMPL:Consuming", int(should_consume_samples));
      logchan_strcimpl->perfItem("SIMPL:Deviation", deviation);
    }

    ///////////////////////////////////////////////////////////////////
    // GENERATE FIXED 64-FRAME OUTPUT
    ///////////////////////////////////////////////////////////////////

    if (should_consume_samples) {
      if (_use_stretching) {
        // Try to get samples from timestretched buffer
        bool have_stretched_samples = _buffered_stretcher.getSamples(outputchan, frames);

        if (have_stretched_samples) {
          // Success - got timestretched samples
          _was_using_stretched = true;
        } else {
          // Fallback: Direct copy from ring buffer if timestretched buffer is empty
          if (current_buffer_size >= size_t(frames)) {
            _oscil->_ringBuffer.pop_many(outputchan, frames);

            // Simple fade-in to prevent clicks when switching to direct copy
            if (_was_using_stretched) {
              int fade_length = std::min(CROSSFADE_LENGTH, frames);
              for (int i = 0; i < fade_length; i++) {
                float t = float(i) / float(fade_length);
                t       = t * t * (3.0f - 2.0f * t); // Smoothstep
                outputchan[i] *= t;
              }
              _was_using_stretched = false;

              // Log fallback only occasionally to avoid spam
              static int fallback_count = 0;
              if (++fallback_count % 20 == 0) {
                logchan_strcimpl->log("ComplexImpl: Fallback to direct copy with fade-in (count: %d)", fallback_count);
              }
            }
          } else {
            // Not enough data anywhere - output silence
            memset(outputchan, 0, frames * sizeof(float));
            logchan_strcimpl->log("ComplexImpl: Underrun - no data available");
            _was_using_stretched = false;
          }
        }

      } else {
        // Direct copy mode (no stretching)
        if (current_buffer_size >= size_t(frames)) {
          _oscil->_ringBuffer.pop_many(outputchan, frames);
        } else {
          memset(outputchan, 0, frames * sizeof(float));
          logchan_strcimpl->log("ComplexImpl: Underrun - buffer:%zu needed:%d", current_buffer_size, frames);
        }
      }
    } else {
      // Output silence (priming or no data)
      memset(outputchan, 0, frames * sizeof(float));
    }
  } // generateOutput
}; // struct ComplexImpl {
} // namespace ork::audio::singularity
