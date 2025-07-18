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

static logchannel_ptr_t logchan_strsimpl = logger()->getChannel("PERF");

struct SimpleImpl {

  StreamingOscillatorBlock* _oscil;
  bool _is_primed = false;

  ////////////////////////////////////////////////////////////////

  SimpleImpl(StreamingOscillatorBlock* osc)
      : _oscil(osc) {
  }

  ////////////////////////////////////////////////////////////////

  void reset() {
    _is_primed = false;
  }

  ////////////////////////////////////////////////////////////////
  // Emergency buffer drain when severely overfull
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
    // compute stretch factor
    ///////////////////////////////////////////////////////////////////

    float deviation = buffer_health - 1.0f;

    // Add dead zone around target level - no stretching within ±5% of target
    static constexpr float DEAD_ZONE = 0.05f; // 5% dead zone
    int stretch_state = 0; // -1, 0, or 1 (sign of deviation)
    if (fabsf(deviation) < DEAD_ZONE) {
      stretch_state = 0;
    } else {
      // Reduce effective deviation by dead zone amount
      if (deviation > 0) {
        stretch_state = -1; // Buffer is overfull, stretch down
      } else {
        stretch_state = 1; // Buffer is underfull, stretch up
      }
    }

    ///////////////////////////////////////////////////////////////////
    // logging
    ///////////////////////////////////////////////////////////////////

    if (1) {
      logchan_strsimpl->perfItem("SIMPL:BufferCur", int(current_buffer_size));
      logchan_strsimpl->perfItem("SIMPL:BufferTgt", int(target_level));
      logchan_strsimpl->perfItem("SIMPL:BufferHealth", buffer_health);
      logchan_strsimpl->perfItem("SIMPL:Consuming", int(should_consume_samples));
      logchan_strsimpl->perfItem("SIMPL:STRETCH", stretch_state);
    }

    ///////////////////////////////////////////////////////////////////
    // GENERATE FIXED 64-FRAME OUTPUT
    ///////////////////////////////////////////////////////////////////

    if (should_consume_samples) {

      constexpr int stretch_count = 1;
      static float discard_samples[stretch_count];

      if(stretch_state == -1){ // Buffer is overfull, stretch down
        stretch_state = (current_buffer_size-stretch_count) > frames ? -1 : 0; // Only stretch if we have enough samples
      }

      // Direct copy mode (no stretching)
      if (current_buffer_size >= size_t(frames)) {
        switch(stretch_state){
          case -1: // Buffer is overfull, stretch down
            // this means discard a few samples 
            //   (assuning we have more than frames+discard amout)
            _oscil->_ringBuffer.pop_many(discard_samples, stretch_count);
            _oscil->_ringBuffer.pop_many(outputchan, frames);
            break;
          case 1: { // Buffer is underfull, stretch up
            // this means we need to repeat some samples
            // by pulling fewer samples than we need
            // and then repeating the last sample
            size_t to_pull = frames-stretch_count;
            _oscil->_ringBuffer.pop_many(outputchan, to_pull);
            for(int i=0; i<stretch_count; i++){
              outputchan[to_pull+i] = outputchan[to_pull-1]; // Repeat last sample
            }
            break;
          }
          default: // No stretching needed
            _oscil->_ringBuffer.pop_many(outputchan, frames);
            break;
        }
      } else {
        memset(outputchan, 0, frames * sizeof(float));
        logchan_strsimpl->log("SimpleImpl: Underrun - buffer:%zu needed:%d", current_buffer_size, frames);
      }
    } else {
      // Output silence (priming or no data)
      memset(outputchan, 0, frames * sizeof(float));
    }
  }
  ////////////////////////////////////////////////////////////////
};
} // namespace ork::audio::singularity
