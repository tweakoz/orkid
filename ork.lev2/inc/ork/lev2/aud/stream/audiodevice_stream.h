////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/lev2_types.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudioFrameCapture {
  std::vector<float> _left;
  std::vector<float> _right;
  int _num_samples = 0;
  int _sample_rate = 0;
  double _timestamp = 0.0;
};

using audioframecapture_ptr_t = std::shared_ptr<AudioFrameCapture>;

///////////////////////////////////////////////////////////////////////////////

class StrAudioDevice : public AudioDevice {
public:
  enum class Mode {
    ASYNC_REALTIME,     // Audio thread, realtime playback
    SYNC_NONREALTIME    // On-demand, deterministic, for offline rendering
  };

  //===========================================
  // LIFECYCLE
  //===========================================
  StrAudioDevice(appinitdata_wkptr_t appinitd);
  ~StrAudioDevice() override;

  void startup() override;
  void shutdown() override;

  //===========================================
  // MODE SELECTION
  //===========================================
  Mode _mode = Mode::ASYNC_REALTIME;

  //===========================================
  // SYNC MODE API (Deterministic/Non-realtime)
  //===========================================
  size_t advanceTime(float dt_seconds);  // Generate audio for time interval

  //===========================================
  // COMMON API
  //===========================================
  audioframecapture_ptr_t extractSamples(int num_samples);
  int availableSamples() const;
  double currentTime() const;

  //===========================================
  // SYNTH INTEGRATION
  //===========================================
  audio::singularity::synth_ptr_t _the_synth;

private:
  //===========================================
  // ASYNC MODE STATE
  //===========================================
  std::thread _audio_thread;
  std::atomic<bool> _thread_running{false};
  void _audioThreadFunc();
  void _startAudioThread();
  void _stopAudioThread();

  //===========================================
  // SYNC MODE STATE
  //===========================================
  double _simulated_time = 0.0;
  double _sample_accumulator = 0.0;  // For fractional samples
  size_t _total_samples_rendered = 0;

  //===========================================
  // COMMON STATE
  //===========================================
  int _sample_rate = 48000;
  int _num_channels = 2;
  // Captured audio buffers
  mutable std::mutex _buffer_mutex;
  std::vector<float> _left_buffer;
  std::vector<float> _right_buffer;

  // Temporary buffer for compute
  std::vector<float> _temp_input_buffer;

  //===========================================
  // INTERNAL
  //===========================================
  void _generateSamples(int num_samples);
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
