////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/config.h>
#include <ork/lev2/lev2_types.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/asset/Asset.h>
#include <ork/util/endian.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/tempstring.h>
#include <ork/dataflow/dataflow.h>
#include <ork/kernel/orkpool.inl>
#include <ork/math/multicurve.h>
#include <ork/math/TransformNode.h>
#include <ork/math/basicfilters.h>
#include <ork/kernel/any.h>
#include <ork/kernel/varmap.inl>
#include <ork/application/application.h>
#include <ork/kernel/concurrent_queue.h>
#include <atomic>
#include <exception>
#include <string>
#include <utility>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudioInputChunk {
  AudioInputChunk(size_t channel_count=1);
  void setNumChannels(size_t channel_count);
  std::vector<input_frames_t> _channels;
  size_t _chunk_index = 0;
  size_t _num_frames = 0;
  double _timestamp = 0.0;  // source timestamp (e.g. hub time)
};

struct AudioInputChunkSource {
  virtual ~AudioInputChunkSource() {}
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual audioinputchunk_ptr_t getChunk() = 0;
};

struct StreamingAudioInputChunkSource : public AudioInputChunkSource {

  void start() final;
  void stop() final;
  lev2::audioinputchunk_ptr_t getChunk() final;
  MpMcBoundedQueue<lev2::audioinputchunk_ptr_t,16> _inputqueue;
  svar64_t _impl;
  int _chunk_index = 0;
  bool _was_reset = true;
  std::atomic<double> _current_playback_timestamp{0.0};  // timestamp of audio currently being played
};

///////////////////////////////////////////////////////////////////////////////

struct AudioDeviceInfo {
  std::string _name;
  std::string _input_short_id;   // e.g., "G6PQ" - 4-char stable hash (empty if no inputs)
  std::string _output_short_id;  // e.g., "H40R" - 4-char stable hash (empty if no outputs)
  int _max_input_channels = 0;
  int _max_output_channels = 0;
  double _sample_rate = 0.0;                      // specific sample rate for this entry
  std::vector<double> _supported_input_rates;     // all supported input sample rates
  std::vector<double> _supported_output_rates;    // all supported output sample rates
  int _device_index = 0;                          // original device index (for grouping)
};
using audiodeviceinfo_ptr_t = std::shared_ptr<AudioDeviceInfo>;
using audiodeviceinfo_list_t = std::vector<audiodeviceinfo_ptr_t>;

audiodeviceinfo_list_t enumerateAudioDevices();
audiodeviceinfo_ptr_t findAudioDeviceByShortId(const std::string& short_id);

// One re-enumeration retry (covers transient busy) for an unresolved short id;
// on continued failure emits a direction-specific diagnostic — including the
// "held by another process" case where the device is present but the requested
// direction reports 0 channels while the other direction is live. Returns the
// resolved device on the retry hit, else nullptr (diagnostic already logged).
audiodeviceinfo_ptr_t retryAndDiagnoseShortId(const std::string& short_id, bool want_output);

///////////////////////////////////////////////////////////////////////////////
// Realtime elevation for the host-api callback thread. The callback thread is
// created by the host api, so its handle only exists from inside the callback:
// call this ONCE, from the first callback invocation, naming the priority band
// the thread should occupy (the singularity job-pool workers sit at 40, so the
// joining callback thread belongs above them).
//
// Platform behavior is documented at the definition (audiodevice.cpp). Both
// platforms emit exactly one line stating the ACHIEVED policy — a denied
// elevation is loud, never silent.
///////////////////////////////////////////////////////////////////////////////

void elevateAudioThread(const char* thread_name, int priority);

///////////////////////////////////////////////////////////////////////////////
// Device-side realtime telemetry. Published lock-free by the audio callback,
// read by diagnostics from any thread (python --diag surfaces, gates).
///////////////////////////////////////////////////////////////////////////////

struct AudioDiagCounters {
  std::atomic<uint64_t> _underflows{0};        // host-reported output underflows (xruns)
  std::atomic<uint64_t> _callbacks{0};         // callback invocations
  std::atomic<uint32_t> _frames_per_buffer{0}; // as delivered by the host api
  std::atomic<float> _sample_rate{0.0f};
};

AudioDiagCounters& audioDiagCounters();

// Thrown by a device startup path that cannot satisfy its preconditions (an
// unresolvable device id, no matching device). Caught in OrkEzApp::_audioInit,
// which degrades to the NULL audio device — the app keeps running without audio
// rather than asserting.
struct AudioDeviceException final : public std::exception {
  AudioDeviceException(std::string msg) : _msg(std::move(msg)) {}
  const char* what() const noexcept override { return _msg.c_str(); }
  std::string _msg;
};

struct AudioDevice {

  static audiodevice_ptr_t createInstance(appinitdata_wkptr_t appinitd);
  static audiodevice_ptr_t createNullInstance(appinitdata_wkptr_t appinitd);
  static audiodevice_ptr_t getInstance();

  AudioDevice(appinitdata_wkptr_t appinitd);
  virtual ~AudioDevice();
  virtual void startup();
  virtual void shutdown();

  appinitdata_wkptr_t _appinitdata;
  svar64_t _impl;
  varmap::varmap_ptr_t _vars;
  audio_input_handler_t _input_handler;
  size_t _num_input_channels = 0;
  size_t _num_output_channels = 0;
  std::string _inp_dev_name;
  std::string _out_dev_name;
  //////////////////
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
