////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/stream/audiodevice_stream.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/kernel/environment.h>
#include <ork/util/logger.h>
#include <chrono>
#include <thread>

namespace ork::lev2 {

using namespace ork::audio::singularity;

static logchannel_ptr_t logchan_straudio =
    logger()->configureChannel("audio.STREAM", fvec3(0.8, 1.0, 0.6), true);

///////////////////////////////////////////////////////////////////////////////

StrAudioDevice::StrAudioDevice(appinitdata_wkptr_t appinitd)
    : AudioDevice(appinitd) {
  logchan_straudio->log("StrAudioDevice created");
}

StrAudioDevice::~StrAudioDevice() {
  if (_mode == Mode::ASYNC_REALTIME) {
    _stopAudioThread();
  }
  logchan_straudio->log("StrAudioDevice destroyed");
}

///////////////////////////////////////////////////////////////////////////////

void StrAudioDevice::startup() {
  // Get configuration from AppInitData
  auto aid = _appinitdata.lock();

  // Get synth instance (same pattern as AudioDevicePa)
  if (aid->_enable_audio_synth) {
    _the_synth = synth::instance();
    logchan_straudio->log("Using audio synth");
  }

  // Configure sample rate and channels from AppInitData
  _sample_rate = aid->_audio_output_numchannels > 0 ? 48000 : 48000;  // Default to 48kHz
  _num_channels = aid->_audio_output_numchannels > 0 ? aid->_audio_output_numchannels : 2;

  // Determine mode: SYNC for offscreen rendering, ASYNC for realtime
  _mode = aid->_audio_stream_sync ? Mode::SYNC_NONREALTIME : Mode::ASYNC_REALTIME;

  logchan_straudio->log("StrAudioDevice::startup SR=%d CH=%d mode=%s",
                        _sample_rate, _num_channels,
                        _mode == Mode::ASYNC_REALTIME ? "ASYNC" : "SYNC");

  // Set sample rate on synth (same pattern as AudioDevicePa::_startupAudio)
  if (_the_synth) {
    _the_synth->setSampleRate(float(_sample_rate));
    logchan_straudio->log("Synth<%p> SR<%d>", (void*)_the_synth.get(), _sample_rate);
  }

  // Allocate input buffer (for synth->compute)
  _temp_input_buffer.resize(8192);  // Max chunk size
  std::fill(_temp_input_buffer.begin(), _temp_input_buffer.end(), 0.0f);

  // Start audio thread if ASYNC mode
  if (_mode == Mode::ASYNC_REALTIME) {
    _startAudioThread();
  }
}

void StrAudioDevice::shutdown() {
  logchan_straudio->log("StrAudioDevice shutdown");
  auto prev_mode = _mode;
  _mode = Mode::INACTIVE;
  if (prev_mode == Mode::ASYNC_REALTIME) {
    _stopAudioThread();
  }
  if (_the_synth) {
    _the_synth = nullptr;
    synth::tearDown();
  }

}

///////////////////////////////////////////////////////////////////////////////
// ASYNC MODE: Audio Thread
///////////////////////////////////////////////////////////////////////////////

void StrAudioDevice::_startAudioThread() {
  _thread_running = true;
  _audio_thread = std::thread([this]() { _audioThreadFunc(); });
  logchan_straudio->log("Audio thread started");
}

void StrAudioDevice::_stopAudioThread() {
  _thread_running = false;
  if (_audio_thread.joinable()) {
    _audio_thread.join();
  }
  logchan_straudio->log("Audio thread stopped");
}

void StrAudioDevice::_audioThreadFunc() {
  // Similar to portaudio callback, but we drive it ourselves
  const int chunk_size = 256;  // ~5ms at 48kHz
  const int sleep_ms = ((chunk_size * 1000) / _sample_rate);

  logchan_straudio->log("Audio thread running: chunk_size=%d sleep_ms=%d", chunk_size, sleep_ms);

  while (_thread_running) {
    _generateSamples(chunk_size);

    // Sleep to match realtime rate
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }

  logchan_straudio->log("Audio thread exiting");
}

///////////////////////////////////////////////////////////////////////////////
// SYNC MODE: On-Demand Generation
///////////////////////////////////////////////////////////////////////////////

size_t StrAudioDevice::advanceTime(float dt_seconds) {
  if( _mode != Mode::SYNC_NONREALTIME ) {
    logchan_straudio->log("advanceTime called in non-SYNC mode!");
    return 0;
  }

  // Calculate EXACTLY how many samples for this time interval
  // Use accumulator to handle fractional samples
  double exact_samples = dt_seconds * _sample_rate;
  _sample_accumulator += exact_samples;

  int samples_to_generate = (int)_sample_accumulator;
  _sample_accumulator -= samples_to_generate;

  // Generate on caller's thread (deterministic)
  _generateSamples(samples_to_generate);
  _total_samples_rendered += samples_to_generate;
  // Advance virtual time
  _simulated_time += dt_seconds;

  if (false) { // Debug logging
    logchan_straudio->log("advanceTime dt=%f exact_samples=%f generated=%d simtime=%f",
                          dt_seconds, exact_samples, samples_to_generate, _simulated_time);
  }
  return _total_samples_rendered;
}

///////////////////////////////////////////////////////////////////////////////
// COMMON: Sample Generation (Core Logic)
///////////////////////////////////////////////////////////////////////////////

void StrAudioDevice::_generateSamples(int num_samples) {
  if (!_the_synth || num_samples == 0) return;

  // ========================================================
  // STEP 1: Call synth->compute (same as patestCallback)
  // ========================================================
  _the_synth->compute(num_samples, _temp_input_buffer.data());

  // ========================================================
  // STEP 2: Read from synth's output buffers
  // ========================================================
  const auto& obuf = _the_synth->_obuf;

  // ========================================================
  // STEP 3: Append to capture buffers (thread-safe)
  // ========================================================
  std::lock_guard<std::mutex> lock(_buffer_mutex);

  for (int i = 0; i < num_samples; i++) {
    _left_buffer.push_back(obuf._leftBuffer[i]);
    _right_buffer.push_back(obuf._rightBuffer[i]);
  }

}

///////////////////////////////////////////////////////////////////////////////
// EXTRACTION: Pull captured audio
///////////////////////////////////////////////////////////////////////////////

audioframecapture_ptr_t StrAudioDevice::extractSamples(int num_samples) {
  std::lock_guard<std::mutex> lock(_buffer_mutex);

  int available = std::min(num_samples, (int)_left_buffer.size());

  auto result = std::make_shared<AudioFrameCapture>();
  result->_left.assign(_left_buffer.begin(),
                       _left_buffer.begin() + available);
  result->_right.assign(_right_buffer.begin(),
                        _right_buffer.begin() + available);
  result->_num_samples = available;
  result->_sample_rate = _sample_rate;
  result->_timestamp = _simulated_time;

  // Remove extracted samples from buffer
  _left_buffer.erase(_left_buffer.begin(),
                    _left_buffer.begin() + available);
  _right_buffer.erase(_right_buffer.begin(),
                     _right_buffer.begin() + available);

  return result;
}

int StrAudioDevice::availableSamples() const {
  std::lock_guard<std::mutex> lock(_buffer_mutex);
  return _left_buffer.size();
}

double StrAudioDevice::currentTime() const {
  return (_mode == Mode::SYNC_NONREALTIME)
         ? _simulated_time
         : 0.0;  // Real-time uses wall clock
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
