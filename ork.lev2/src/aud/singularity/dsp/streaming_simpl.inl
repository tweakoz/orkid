////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/aud/singularity/fft.h>
#include <ork/lev2/aud/singularity/audiotest.h>
#include <dspstretch/signalsmith-stretch.h>

namespace ork::audio::singularity {

static logchannel_ptr_t logchan_strsimpl = logger()->configureChannel("STRSIMPLE", fvec3(1, 0.6, .8), true);

struct SimpleImpl {

  StreamingOscillatorBlock* _oscil;
  bool _is_primed = false;
  size_t _exec_count = 0;
  float _read_position_L = 0.0f;  // Fractional sample position for L channel
  float _read_position_R = 0.0f;  // Fractional sample position for R channel

  double _latest_chunk_timestamp = 0.0;  // timestamp from most recently queued chunk

  // Stored values for lambda-based perfItems
  float _deviation = 0.0f;
  float _correction = 0.0f;
  int _push_count = 0;
  int _chunkindex = 0;
  int _pop_count = 0;
  int _drained_samples = 0;
  int _buf_current = 0;
  int _buf_target = 0;
  float _buf_health = 0.0f;
  int _consuming = 0;
  float _playback_rate = 1.0f;
  float _ed_current_size = 0.0f;
  float _ed_ts_1_5 = 0.0f;
  int _ed_excess = 0;
  int _ed_to_drain = 0;

  // Latency probe (active when CHIRP pattern detected)
  std::unique_ptr<TestPatternProbe> _probe;
  uint64_t _probe_sample_count = 0;
  uint64_t _probe_report_interval = 0; // samples between reports (set at init)
  double _probe_latency_sum = 0.0;
  int _probe_latency_count = 0;

  ////////////////////////////////////////////////////////////////

  SimpleImpl(StreamingOscillatorBlock* osc)
      : _oscil(osc) {

      logchan_strsimpl->_perf_interval = 0.001f;

      // Register lambda-based perfItems
      logchan_strsimpl->perfItem("deviation", (float_lambda_t) [this]() -> float { return _deviation; });
      logchan_strsimpl->perfItem("correction", (float_lambda_t) [this]() -> float { return _correction; });
      logchan_strsimpl->perfItem("CIDX", (int_lambda_t) [this]() -> int { return _chunkindex; });
      logchan_strsimpl->perfItem("PUSH", (int_lambda_t) [this]() -> int { return _push_count; });
      logchan_strsimpl->perfItem("POP", (int_lambda_t)[this]() -> int { return _pop_count; });
      logchan_strsimpl->perfItem("SIMPL:DrainedSamples", [this]() -> int { return _drained_samples; });
      logchan_strsimpl->perfItem("BufCur", (int_lambda_t)[this]() -> int { return _buf_current; });
      logchan_strsimpl->perfItem("BufTgt", (int_lambda_t)[this]() -> int { return _buf_target; });
      logchan_strsimpl->perfItem("BufHealth", (float_lambda_t) [this]() -> float { return _buf_health; });
      logchan_strsimpl->perfItem("Consuming", (int_lambda_t)[this]() -> int { return _consuming; });
      logchan_strsimpl->perfItem("PlaybackRate", (float_lambda_t) [this]() -> float { return _playback_rate; });
      logchan_strsimpl->perfItem("ED.current_size", (float_lambda_t) [this]() -> float { return _ed_current_size; });
      logchan_strsimpl->perfItem("ED.ts*1.5", (float_lambda_t) [this]() -> float { return _ed_ts_1_5; });
      logchan_strsimpl->perfItem("ED.excess", (int_lambda_t)[this]() -> int { return _ed_excess; });
      logchan_strsimpl->perfItem("ED.to_drain", (int_lambda_t)[this]() -> int { return _ed_to_drain; });

      // Initialize latency probe (3-second reporting interval)
      float sr = synth::instance() ? synth::instance()->_sampleRate : 48000.0f;
      _probe = std::make_unique<TestPatternProbe>(sr, 4096);
      _probe->setChirpConfig(ChirpConfig()); // default chirp params
      _probe_report_interval = uint64_t(sr * 3.0); // every 3 seconds
  }

  ////////////////////////////////////////////////////////////////

  void reset() {
    _is_primed = false;
    _read_position_L = 0.0f;
    _read_position_R = 0.0f;
  }

  ////////////////////////////////////////////////////////////////
  // Emergency buffer drain when severely overfull
  ////////////////////////////////////////////////////////////////

  float computePlaybackRate(float buffer_health) {
    _deviation = buffer_health - 1.0f;

    // Tighter dead zone ±2% (reduced from 5%)
    static constexpr float DEAD_ZONE = 0.02f;
    if (fabsf(_deviation) < DEAD_ZONE) {
      //return 1.0f;  // Normal playback
    }

    // Stronger correction - max ±1.0% pitch deviation (increased from 0.5%)
    static constexpr float MAX_CORRECTION = 0.005f;

    // Stronger proportional control (increased from 0.1 to 0.3)
    _correction = -_deviation * 0.03f;  // 30% feedback strength

    _correction = std::clamp(_correction, -MAX_CORRECTION, MAX_CORRECTION);

    return 1.0f - _correction;
  }

  ////////////////////////////////////////////////////////////////

  void emergencyDrain() {
    size_t current_size = _oscil->_ringBuffer.size();
    size_t target_size  = _oscil->_dynamic_target_level;
    size_t drain_threshold  = size_t(target_size*4.5);                  // Exit priming at target level
    if (current_size > drain_threshold) { // More aggressive trigger

      size_t excess   = current_size - target_size;
      size_t to_drain = std::min(excess, size_t(16384)); // Drain max 16K samples at once

      std::vector<float> temp_drain(to_drain);
      _oscil->_ringBuffer.pop_many(temp_drain.data(), to_drain);

      // Also drain R channel if stereo
      if (_oscil->_num_channels == 2) {
        _oscil->_ringBuffer_R.pop_many(temp_drain.data(), to_drain);
      }

      _ed_current_size = float(current_size);
      _ed_ts_1_5 = drain_threshold;
      _ed_excess = int(excess);
      _ed_to_drain = int(to_drain);

      logchan_strsimpl->log(
          "EDRAIN: cursize<%zu> thresh<%zu> excess<%zu> target<%zu> removing %zu samples, buffer now %zu",
          current_size,
          drain_threshold,
          excess,
          target_size,
          to_drain,
          _oscil->_ringBuffer.size());
      _drained_samples = int(to_drain);
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
      logchan_strsimpl->log("SimpleImpl: Detected source reset, clearing ring buffers");
      _oscil->_ringBuffer.clear();
      _oscil->_ringBuffer_R.clear();
      source->_was_reset = false;
      reset();
    }
    while (source->_inputqueue.try_pop(chunk)) {
      _chunkindex = source->_chunk_index;
      _latest_chunk_timestamp = chunk->_timestamp;
      auto& chan_L       = chunk->_channels[0];
      size_t num_samples = chan_L.size();
      const float* src_L = chan_L.data();

      // Push L channel to ring buffer
      size_t available_space = _oscil->_ringBuffer.capacity() - _oscil->_ringBuffer.size();
      size_t to_push         = std::min(available_space, num_samples);

      if (to_push > 0) {
        _push_count++;
        _oscil->_ringBuffer.push_many(src_L, to_push);

        // Push R channel if stereo
        if (_oscil->_num_channels == 2 && chunk->_channels.size() >= 2) {
          auto& chan_R       = chunk->_channels[1];
          const float* src_R = chan_R.data();
          _oscil->_ringBuffer_R.push_many(src_R, to_push);
        }
      }

      if (to_push < num_samples) {
        logchan_strsimpl->log("SimpleImpl: Dropped %zu samples", num_samples - to_push);
      }
    }
  }

  ////////////////////////////////////////////////////////////////

  void generateOutput(DspBuffer& dspbuf) {
    auto outputchan_L          = _oscil->getOutBuf(dspbuf, 0) + _oscil->_layer->_dspwritebase;
    float* outputchan_R        = nullptr;
    bool is_stereo             = (_oscil->_num_channels == 2);
    if (is_stereo) {
      outputchan_R = _oscil->getOutBuf(dspbuf, 1) + _oscil->_layer->_dspwritebase;
    }

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
    // Update perfItem values
    ///////////////////////////////////////////////////////////////////

    _buf_current = int(current_buffer_size);
    _buf_target = int(target_level);
    _buf_health = buffer_health;
    _consuming = int(should_consume_samples);
    _playback_rate = playback_rate;

    ///////////////////////////////////////////////////////////////////
    // GENERATE FIXED 64-FRAME OUTPUT WITH INTERPOLATION
    ///////////////////////////////////////////////////////////////////

    size_t popped = 0;

    if (should_consume_samples) {

      // Calculate samples needed for interpolation
      size_t samples_needed = size_t(frames * playback_rate) + 2;  // +2 for interpolation safety

      if (current_buffer_size >= samples_needed) {
        // Pre-fetch L channel buffer data for interpolation
        std::vector<float> temp_buffer_L(samples_needed);
        _oscil->_ringBuffer.peek_many(temp_buffer_L.data(), samples_needed);

        // Pre-fetch R channel if stereo
        std::vector<float> temp_buffer_R;
        if (is_stereo) {
          temp_buffer_R.resize(samples_needed);
          _oscil->_ringBuffer_R.peek_many(temp_buffer_R.data(), samples_needed);
        }

        // Generate L channel output with linear interpolation
        for (int i = 0; i < frames; i++) {
          float read_pos = _read_position_L + i * playback_rate;

          int index = int(read_pos);
          float frac = read_pos - float(index);

          // Linear interpolation for L channel
          float sample0 = temp_buffer_L[index];
          float sample1 = temp_buffer_L[index + 1];
          outputchan_L[i] = sample0 + frac * (sample1 - sample0);
        }

        // Generate R channel output if stereo
        if (is_stereo) {
          for (int i = 0; i < frames; i++) {
            float read_pos = _read_position_R + i * playback_rate;

            int index = int(read_pos);
            float frac = read_pos - float(index);

            // Linear interpolation for R channel
            float sample0 = temp_buffer_R[index];
            float sample1 = temp_buffer_R[index + 1];
            outputchan_R[i] = sample0 + frac * (sample1 - sample0);
          }
        }

        // Feed output to latency probe and report periodically
        _probe->write(outputchan_L, frames);
        _probe_sample_count += frames;
        if (_probe->ready()) {
          double lat = _probe->measureLatencyMs();
          if (lat >= 0.0) {
            _probe_latency_sum += lat;
            _probe_latency_count++;
          }
        }
        if (_probe_report_interval > 0 && _probe_sample_count >= _probe_report_interval) {
          if (_probe_latency_count > 0) {
            double avg = _probe_latency_sum / double(_probe_latency_count);
            logchan_strsimpl->log("LATENCY PROBE: avg=%.1f ms (%d measurements)", avg, _probe_latency_count);
          }
          _probe_sample_count = 0;
          _probe_latency_sum = 0.0;
          _probe_latency_count = 0;
        }

        // Advance read positions
        _read_position_L += frames * playback_rate;
        if (is_stereo) {
          _read_position_R += frames * playback_rate;
        }

        // Update playback timestamp on the source (safe for main-thread reads)
        {
          auto source = _oscil->_streamingdata->_source;
          if (source) {
            source->_current_playback_timestamp.store(_latest_chunk_timestamp, std::memory_order_relaxed);
          }
        }

        // Consume integer samples from ring buffers
        int samples_consumed = int(_read_position_L);
        if (samples_consumed > 0) {
          std::vector<float> discard(samples_consumed);
          popped = size_t(samples_consumed);
          _oscil->_ringBuffer.pop_many(discard.data(), samples_consumed);
          _read_position_L -= float(samples_consumed);  // Keep fractional part

          if (is_stereo) {
            _oscil->_ringBuffer_R.pop_many(discard.data(), samples_consumed);
            _read_position_R -= float(samples_consumed);
          }
        }

      } else {
        // Underrun - output silence
        memset(outputchan_L, 0, frames * sizeof(float));
        if (is_stereo) {
          memset(outputchan_R, 0, frames * sizeof(float));
        }
        logchan_strsimpl->log("SimpleImpl: Underrun - buffer:%zu needed:%zu", current_buffer_size, samples_needed);
      }
      _pop_count++;
    } else {
      // Output silence (priming or no data)
      memset(outputchan_L, 0, frames * sizeof(float));
      if (is_stereo) {
        memset(outputchan_R, 0, frames * sizeof(float));
      }
    }
  }
  ////////////////////////////////////////////////////////////////
};
} // namespace ork::audio::singularity
