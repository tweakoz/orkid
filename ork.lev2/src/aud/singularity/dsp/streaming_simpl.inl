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

static logchannel_ptr_t logchan_strsimpl = logger()->configureChannel("STRSIMPLE", fvec3(1, 0.6, .8), true);

struct SimpleImpl {

  StreamingOscillatorBlock* _oscil;
  bool _is_primed = false;
  size_t _exec_count = 0;
  float _read_position = 0.0f;  // Fractional sample position for interpolation

  ////////////////////////////////////////////////////////////////

  SimpleImpl(StreamingOscillatorBlock* osc)
      : _oscil(osc) {
  }

  ////////////////////////////////////////////////////////////////

  void reset() {
    _is_primed = false;
    _read_position = 0.0f;
  }

  ////////////////////////////////////////////////////////////////
  // Emergency buffer drain when severely overfull
  ////////////////////////////////////////////////////////////////

  float computePlaybackRate(float buffer_health) {
    float deviation = buffer_health - 1.0f;

    // Tighter dead zone ±2% (reduced from 5%)
    static constexpr float DEAD_ZONE = 0.02f;
    if (fabsf(deviation) < DEAD_ZONE) {
      return 1.0f;  // Normal playback
    }

    // Stronger correction - max ±1.0% pitch deviation (increased from 0.5%)
    static constexpr float MAX_CORRECTION = 0.001f;

    // Stronger proportional control (increased from 0.1 to 0.3)
    float correction = -deviation * 0.3f;  // 30% feedback strength
    correction = std::clamp(correction, -MAX_CORRECTION, MAX_CORRECTION);

    return 1.0f - correction;
  }

  ////////////////////////////////////////////////////////////////

  void emergencyDrain() {
    size_t current_size = _oscil->_ringBuffer.size();
    size_t target_size  = _oscil->_dynamic_target_level;

    if (current_size > target_size * 1.5) { // More aggressive trigger
      size_t excess   = current_size - target_size;
      size_t to_drain = std::min(excess, size_t(16384)); // Drain max 16K samples at once

      std::vector<float> temp_drain(to_drain);
      _oscil->_ringBuffer.pop_many(temp_drain.data(), to_drain);

      logchan_strsimpl->log(
          "EMERGENCY DRAIN: Removed %zu samples, buffer now %zu (target %zu)", to_drain, _oscil->_ringBuffer.size(), target_size);
      logchan_strsimpl->perfItem("SIMPL:DrainedSamples", int(to_drain));
    }
  }

  ////////////////////////////////////////////////////////////////

  void processInput() {
    auto source = _oscil->_streamingdata->_source;
    if (!source)
      return;

    // Process all available chunks
    lev2::audioinputchunk_ptr_t chunk;
    bool was_reset = source->_was_reset;
    if(was_reset){
      logchan_strsimpl->log("SimpleImpl: Detected source reset, clearing ring buffer");
      _oscil->_ringBuffer.clear();
      source->_was_reset = false;
      reset();
    }
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
        logchan_strsimpl->log("SimpleImpl: Dropped %zu samples", num_samples - to_push);
      }
    }
  }

  ////////////////////////////////////////////////////////////////

  void generateOutput(DspBuffer& dspbuf) {
    auto outputchan            = _oscil->getOutBuf(dspbuf, 0) + _oscil->_layer->_dspwritebase;
    int frames                 = _oscil->_layer->_dspwritecount; // Always 64 - immutable
    size_t current_buffer_size = _oscil->_ringBuffer.size();
    int ecount = _exec_count++;
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
      logchan_strsimpl->log("SimpleImpl: Priming complete - buffer:%zu target:%zu", current_buffer_size, target_level);
    } else if (_is_primed && current_buffer_size < enter_threshold) {
      _is_primed = false;
      logchan_strsimpl->log(
          "SimpleImpl: Re-entering priming - buffer:%zu (%.1f%% of target)",
          current_buffer_size,
          float(current_buffer_size) / float(target_level) * 100.0f);
    }

    bool should_consume_samples = (current_buffer_size > enter_threshold) and _is_primed;

    float buffer_health = float(current_buffer_size) / float(target_level);

    ///////////////////////////////////////////////////////////////////
    // compute playback rate for interpolation
    ///////////////////////////////////////////////////////////////////

    float playback_rate = computePlaybackRate(buffer_health);

    ///////////////////////////////////////////////////////////////////
    // logging
    ///////////////////////////////////////////////////////////////////

    if (1){
      if((ecount%128)==0){
        logchan_strsimpl->perfItem("BufCur",int(current_buffer_size));
        logchan_strsimpl->perfItem("BufTgt",int(target_level));
        logchan_strsimpl->perfItem("BufHealth",buffer_health);
        logchan_strsimpl->perfItem("Consuming",int(should_consume_samples));
        logchan_strsimpl->perfItem("PlaybackRate",playback_rate);
      }
    }

    ///////////////////////////////////////////////////////////////////
    // GENERATE FIXED 64-FRAME OUTPUT WITH INTERPOLATION
    ///////////////////////////////////////////////////////////////////

    if (should_consume_samples) {

      // Calculate samples needed for interpolation
      size_t samples_needed = size_t(frames * playback_rate) + 2;  // +2 for interpolation safety

      if (current_buffer_size >= samples_needed) {
        // Pre-fetch buffer data for interpolation
        std::vector<float> temp_buffer(samples_needed);
        _oscil->_ringBuffer.peek_many(temp_buffer.data(), samples_needed);

        // Generate output with linear interpolation
        for (int i = 0; i < frames; i++) {
          float read_pos = _read_position + i * playback_rate;

          int index = int(read_pos);
          float frac = read_pos - float(index);

          // Linear interpolation
          float sample0 = temp_buffer[index];
          float sample1 = temp_buffer[index + 1];
          outputchan[i] = sample0 + frac * (sample1 - sample0);
        }

        // Advance read position
        _read_position += frames * playback_rate;

        // Consume integer samples from ring buffer
        int samples_consumed = int(_read_position);
        if (samples_consumed > 0) {
          std::vector<float> discard(samples_consumed);
          _oscil->_ringBuffer.pop_many(discard.data(), samples_consumed);
          _read_position -= float(samples_consumed);  // Keep fractional part
        }

      } else {
        // Underrun
        memset(outputchan, 0, frames * sizeof(float));
        logchan_strsimpl->log("SimpleImpl: Underrun - buffer:%zu needed:%zu", current_buffer_size, samples_needed);
      }
    } else {
      // Output silence (priming or no data)
      memset(outputchan, 0, frames * sizeof(float));
    }
  }
  ////////////////////////////////////////////////////////////////
};
} // namespace ork::audio::singularity
