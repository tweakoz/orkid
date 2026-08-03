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
#include <cmath>
#include <thread>
#include <sndfile.h>

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
  _closeWav();
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

  // WAV tee configuration (opened lazily on the first tee'd generate).
  _wav_out_path = aid->_audio_wav_out;
  if (not _wav_out_path.empty()) {
    // A WAV path with no synth to generate audio is a misconfiguration — fail loudly.
    if (not _the_synth) {
      logerrchannel()->log(
          "StrAudioDevice: wav_output_path<%s> requested but no audio synth is enabled",
          _wav_out_path.c_str());
      OrkAssert(false);
    }
    // Realtime playback must never do file I/O on the audio thread — the tee is
    // a SYNC-mode (offline) feature only. Warn once and disable it.
    if (_mode == Mode::ASYNC_REALTIME) {
      logerrchannel()->log(
          "StrAudioDevice: wav_output_path<%s> ignored in ASYNC_REALTIME mode "
          "(WAV tee is SYNC_NONREALTIME only)",
          _wav_out_path.c_str());
      _wav_out_path.clear();
    }
  }

  // Start audio thread if ASYNC mode
  if (_mode == Mode::ASYNC_REALTIME) {
    _startAudioThread();
  }
}

void StrAudioDevice::shutdown() {
  logchan_straudio->log("StrAudioDevice shutdown");
  auto prev_mode = _mode;
  // INACTIVE first: any advanceTime() not yet inside _generateSamples early-outs.
  _mode = Mode::INACTIVE;
  if (prev_mode == Mode::ASYNC_REALTIME) {
    _stopAudioThread();
  }
  // Drop the synth under _synth_mutex. Acquiring the lock waits for any
  // in-flight _generateSamples (SYNC-mode pump runs advanceTime on a worker
  // thread) to finish; nulling the pointer under the lock guarantees no later
  // _generateSamples touches the synth. Only then is synth::tearDown() (which
  // clears/frees the voice layers) safe — the pre-fix race here was a UAF:
  // the aux worker's compute() ran against layers deinit() was destroying.
  synth_ptr_t doomed;
  {
    std::lock_guard<std::mutex> synthlock(_synth_mutex);
    doomed      = _the_synth;
    _the_synth  = nullptr;
  }
  if (doomed) {
    doomed = nullptr;
    synth::tearDown();
  }
  // Close the WAV tee last, after all generation has quiesced (idempotent).
  _closeWav();
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

  // EXACT sample accounting. dt arrives as a float, so a caller-intended whole
  // number of samples (1/60 s at 48kHz IS 800 samples) lands here as
  // 800.0000417 — a bias the fractional accumulator integrates until, around
  // update 24k, one call asks for 801 samples. A chunk size that is not a
  // multiple of frames_per_controlpass destroys fixed-step determinism (and
  // used to overrun the synth's buffers outright). So: if the requested
  // interval is a whole sample count to within the precision the caller could
  // possibly have expressed it in, it IS that whole sample count. Anything
  // genuinely fractional still goes through the accumulator, which stays exact
  // because a snapped interval leaves no residue to carry.
  double exact_samples = double(dt_seconds) * double(_sample_rate);
  double whole_samples = std::round(exact_samples);
  // 2^-23 == one float mantissa ulp, scaled by the magnitude of the interval:
  // the widest the narrowing of the caller's dt to float can have moved it.
  double snap_tolerance = std::abs(exact_samples) * 0x1p-23;
  if (std::abs(exact_samples - whole_samples) <= snap_tolerance) {
    exact_samples = whole_samples;
  }
  _sample_accumulator += exact_samples;

  int samples_to_generate = int(std::floor(_sample_accumulator));
  _sample_accumulator -= double(samples_to_generate);

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
  if (num_samples == 0) return;

  // Hold _synth_mutex across the WHOLE synth access (compute + tee + capture).
  // shutdown() takes this same lock before dropping the synth, so it can never
  // free the voice layers while compute() is walking them.
  std::lock_guard<std::mutex> synthlock(_synth_mutex);
  if (!_the_synth) return;

  // ========================================================
  // STEP 1: Call synth->compute (same as patestCallback)
  // ========================================================
  _the_synth->compute(num_samples, _temp_input_buffer.data());

  // ========================================================
  // STEP 2: Read from synth's output buffers
  // ========================================================
  const auto& obuf = _the_synth->_obuf;

  // ========================================================
  // STEP 3: WAV tee (generation-time, SYNC_NONREALTIME only)
  // ========================================================
  if (_mode == Mode::SYNC_NONREALTIME and not _wav_out_path.empty()) {
    _teeToWav(num_samples);
  }

  // ========================================================
  // STEP 4: Append to capture buffers (thread-safe wrt extractSamples)
  // ========================================================
  std::lock_guard<std::mutex> lock(_buffer_mutex);

  for (int i = 0; i < num_samples; i++) {
    _left_buffer.push_back(obuf._leftBuffer[i]);
    _right_buffer.push_back(obuf._rightBuffer[i]);
  }

}

///////////////////////////////////////////////////////////////////////////////
// WAV TEE: generation-time capture to disk (SYNC_NONREALTIME only).
// Caller holds _synth_mutex; _the_synth is non-null.
///////////////////////////////////////////////////////////////////////////////

void StrAudioDevice::_teeToWav(int num_samples) {
  // libsndfile only ever sees 2 interleaved channels — the synth output is L/R.
  constexpr int kWavChannels = 2;

  if (nullptr == _wav_file) {
    SF_INFO sfinfo;
    memset(&sfinfo, 0, sizeof(sfinfo));
    sfinfo.samplerate = _sample_rate;  // engine SR — never hardcoded
    sfinfo.channels   = kWavChannels;
    sfinfo.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    _wav_file         = sf_open(_wav_out_path.c_str(), SFM_WRITE, &sfinfo);
    if (nullptr == _wav_file) {
      logerrchannel()->log(
          "StrAudioDevice: cannot open wav_output_path<%s> for write: %s",
          _wav_out_path.c_str(),
          sf_strerror(nullptr));
      OrkAssert(false);
    }
    // The PEAK chunk embeds a wall-clock creation timestamp, which would break
    // byte-for-byte determinism of the rendered file. Suppress it.
    sf_command(_wav_file, SFC_SET_ADD_PEAK_CHUNK, nullptr, SF_FALSE);
    logchan_straudio->log(
        "StrAudioDevice: WAV tee open<%s> SR=%d CH=%d",
        _wav_out_path.c_str(), _sample_rate, kWavChannels);
  }

  const auto& obuf = _the_synth->_obuf;
  _wav_interleave.resize(size_t(num_samples) * kWavChannels);
  for (int i = 0; i < num_samples; i++) {
    _wav_interleave[i * kWavChannels + 0] = obuf._leftBuffer[i];
    _wav_interleave[i * kWavChannels + 1] = obuf._rightBuffer[i];
  }
  sf_count_t wrote = sf_writef_float(_wav_file, _wav_interleave.data(), num_samples);
  if (wrote != num_samples) {
    logerrchannel()->log(
        "StrAudioDevice: short WAV write (%lld of %d frames) to<%s>",
        (long long)wrote, num_samples, _wav_out_path.c_str());
    OrkAssert(false);
  }
}

void StrAudioDevice::_closeWav() {
  if (_wav_file) {
    sf_close(_wav_file);
    _wav_file = nullptr;
    logchan_straudio->log("StrAudioDevice: WAV tee closed<%s>", _wav_out_path.c_str());
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
