////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <map>
#include <vector>

#include <ork/kernel/thread.h>
#include <ork/lev2/aud/audiodevice.h>
#include <CoreServices/CoreServices.h>
#include <CoreMIDI/CoreMIDI.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <ork/kernel/orkpool.inl>

namespace ork::lev2::ca {

struct CoreAudioDevice;
struct CoreAudioDeviceImpl;
struct AuContext;

using cadevice_ptr_t      = std::shared_ptr<CoreAudioDevice>;
using cadevice_impl_ptr_t = std::shared_ptr<CoreAudioDeviceImpl>;
using aucontext_ptr_t     = std::shared_ptr<AuContext>;

///////////////////////////////////////////////////////////////////////////////

struct CoreAudioDeviceInfo {

  CoreAudioDeviceInfo(AudioDeviceID did, bool want_inputs);
  int countChannels();

  AudioDeviceID _ID = kAudioDeviceUnknown;
  bool _isInput     = false;
  std::string _name;
  UInt32 _safetyOffset;
  UInt32 _bufferSizeFrames;
  AudioStreamBasicDescription _format;
};
using coreaudio_device_info_ptr_t = std::shared_ptr<CoreAudioDeviceInfo>;

///////////////////////////////////////////////////////////////////////////////

struct CoreAudioDeviceImpl {

  CoreAudioDeviceImpl(coreaudio_device_info_ptr_t info);

  bool isValid() const;

  void SetBufferSize(UInt32 size);

  coreaudio_device_info_ptr_t _info;
};

///////////////////////////////////////////////////////////////////////////////

class AudioDeviceList {
public:
  using device_map_t = std::map<std::string, coreaudio_device_info_ptr_t>;

  struct Device {
    char mName[64];
    AudioDeviceID _ID;
  };

  AudioDeviceList(bool inputs);
  ~AudioDeviceList();

  const device_map_t& GetMap() const {
    return _devices;
  }

  void BuildList();
  void EraseList();

  bool mInputs;

  device_map_t _devices;
};

///////////////////////////////////////////////////////////////////////////////

struct CoreAudioDevice : public AudioDevice {

  CoreAudioDevice(appinitdata_wkptr_t appinitd);
  virtual void startup();
  virtual void shutdown();
  coreaudio_device_info_ptr_t _input_info;
  coreaudio_device_info_ptr_t _output_info;
  cadevice_impl_ptr_t _input_impl;
  cadevice_impl_ptr_t _output_impl;

  AudioDeviceList _inputDevList;
  AudioDeviceList _outputDevList;

  int _actual_input_channels = 0;  // actual device channel count (may differ from requested)

  aucontext_ptr_t _aucontext;
  thread_ptr_t _au_thread;
  audio::singularity::synth_ptr_t _the_synth;
  std::vector<float> _noinputblock;

  std::atomic<float> _cpu_load_accumulator{0.0f};
  std::atomic<int> _cpu_load_sample_count{0};
  std::chrono::high_resolution_clock::time_point _last_cpu_load_reset;
  static constexpr int CPU_LOAD_AVERAGE_SAMPLES = 100;

  float calculateCPULoad();
};

} // namespace ork::lev2::ca
