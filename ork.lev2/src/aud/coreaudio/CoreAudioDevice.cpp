////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>
#if defined(ENABLE_CORE_AUDIO)

//#define DEBUG_LATENCY

// Uncomment to force the "no valid audio device" path on the next app
// launch: clears both input and output selections right before the tier-4
// check, triggering the macOS dialog + OrkAssert. Test without unplugging
// hardware. Remember to re-comment before shipping.
//#define DEBUG_NO_DEVICE_DIALOGUE

#include "CoreAudioDevice.h"
#include "au.h"
#include "ca_helpers/CARingBuffer.h"
#include "ca_helpers/CAStreamBasicDescription.h"
#include <libkern/OSAtomic.h>
#include <ork/util/logger.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/audiotest.h>
#include <mach/mach_time.h>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::ca {
///////////////////////////////////////////////////////////////////////////////
using namespace ork::audio::singularity;

static logchannel_ptr_t logchan_coreaudio = logger()->configureChannel("CoreAudio", fvec3(1, 0.6, .8), true);

///////////////////////////////////////////////////////////////////////////////
// Thread CPU time, paired with wall time in the first-window diagnostics — the
// darwin twin of the helper in audiodevice_pa.cpp: wall >> cpu means the audio
// thread was DESCHEDULED (a scheduling problem), wall ~= cpu means the compute
// itself overran the deadline (a workload problem).
// darwin serves CLOCK_THREAD_CPUTIME_ID out of thread_info(THREAD_BASIC_INFO)
// user_time+system_time, so it is the same quantity linux reports but quantized
// to 1us. That is fine for a fence in the hundreds-of-us range and coarse for
// anything finer — do not read single-microsecond differences off it.
///////////////////////////////////////////////////////////////////////////////
static inline uint64_t _threadCpuNanos() {
  timespec ts{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return uint64_t(ts.tv_sec) * 1000000000ull + uint64_t(ts.tv_nsec);
}

void EnumerateMidiDevices() {
  int n                = MIDIGetNumberOfExternalDevices();
  MIDIEntityRef entity = 0;

  CFStringRef pname, pmanuf, pmodel;
  char name[64], manuf[64], model[64];
  for (int i = 0; i < n; ++i) {
    MIDIDeviceRef dev = MIDIGetExternalDevice(i);

    MIDIObjectGetStringProperty(dev, kMIDIPropertyName, &pname);
    MIDIObjectGetStringProperty(dev, kMIDIPropertyManufacturer, &pmanuf);
    MIDIObjectGetStringProperty(dev, kMIDIPropertyModel, &pmodel);
    CFStringGetCString(pname, name, sizeof(name), 0);
    CFStringGetCString(pmanuf, manuf, sizeof(manuf), 0);
    CFStringGetCString(pmodel, model, sizeof(model), 0);

    logchan_coreaudio->log("MidiDevice<%d> Name<%s>", i, name);
    CFRelease(pname);
    CFRelease(pmanuf);
    CFRelease(pmodel);
  }
}

///////////////////////////////////////////////////////////////////////////////

AudioDeviceList::AudioDeviceList(bool inputs)
    : mInputs(inputs) {
  BuildList();
}

///////////////////////////////////////////////////////////////////////////////

AudioDeviceList::~AudioDeviceList() {
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceList::BuildList() {
  // mDevices.clear();

  UInt32 propsize;

  __Verify_noErr(AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &propsize, NULL));
  int nDevices          = propsize / sizeof(AudioDeviceID);
  AudioDeviceID* devids = new AudioDeviceID[nDevices];
  __Verify_noErr(AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &propsize, devids));

  for (int i = 0; i < nDevices; ++i) {
    auto info = std::make_shared<CoreAudioDeviceInfo>(devids[i], mInputs);
    if (info->countChannels() > 0) {
      Device d;
      d._ID = devids[i];

      UInt32 name_len = sizeof(d.mName);
      __Verify_noErr(AudioDeviceGetProperty(d._ID, 0, info->_isInput, kAudioDevicePropertyDeviceName, &name_len, d.mName));

      auto c_str = (const char*)(&d.mName[0]);
      std::string name(c_str);

      info->_name = name;
      info->_ID   = d._ID;

      if (info->_ID == kAudioDeviceUnknown)
        continue;

      UInt32 propsize;
      propsize = sizeof(UInt32);
      __Verify_noErr(
          AudioDeviceGetProperty(info->_ID, 0, info->_isInput, kAudioDevicePropertySafetyOffset, &propsize, &info->_safetyOffset));
      propsize = sizeof(UInt32);
      __Verify_noErr(AudioDeviceGetProperty(
          info->_ID, 0, info->_isInput, kAudioDevicePropertyBufferFrameSize, &propsize, &info->_bufferSizeFrames));
      propsize = sizeof(AudioStreamBasicDescription);
      __Verify_noErr(
          AudioDeviceGetProperty(info->_ID, 0, info->_isInput, kAudioDevicePropertyStreamFormat, &propsize, &info->_format));

      _devices[name] = info;
    }
  }
  delete[] devids;
}

///////////////////////////////////////////////////////////////////////////////
// File-local helpers for tiered audio-device selection.
//
// Orkid requires:
//   • OUTPUT: stereo (2 channels) at 48 kHz
//   • INPUT : 1 or 2 channels at 48 kHz
//
// Selection order (per direction):
//   tier 1 — ORKID_AUDIO_{INPUT,OUTPUT}_DEVICE env var, matched against the
//            enumerated device list with the constraints above applied.
//   tier 2 — CoreAudio's reported system default device, if it passes the
//            constraint filter. This tier gives us headphones-vs-speakers
//            and mic-preference for free because macOS already tracks
//            headphone insertion / default-mic selection at the OS level.
//   tier 3 — first device in the enumeration that passes the filter.
//   tier 4 — no suitable device AND the direction is enabled: pop a macOS
//            dialog and assert. Skipped if the direction is disabled.
///////////////////////////////////////////////////////////////////////////////

namespace {

constexpr int kDesiredSampleRate = 48000;

static bool _matchesConstraints(
    const std::shared_ptr<CoreAudioDeviceInfo>& info,
    bool is_input) {
  if (!info) return false;
  int rate = int(info->_format.mSampleRate);
  if (rate != kDesiredSampleRate) return false;
  int ch = info->countChannels();
  if (is_input) return (ch == 1 || ch == 2);
  return (ch == 2);
}

static std::shared_ptr<CoreAudioDeviceInfo>
_findByName(const AudioDeviceList& devlist, const std::string& name, bool is_input) {
  if (name.empty()) return nullptr;
  for (const auto& kv : devlist.GetMap()) {
    if (kv.first == name && _matchesConstraints(kv.second, is_input)) {
      return kv.second;
    }
  }
  return nullptr;
}

static AudioDeviceID _getSystemDefaultID(bool is_input) {
  AudioDeviceID id = kAudioDeviceUnknown;
  UInt32 sz        = sizeof(id);
  AudioObjectPropertyAddress addr = {
      is_input ? kAudioHardwarePropertyDefaultInputDevice
               : kAudioHardwarePropertyDefaultOutputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain};
  OSStatus rc = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &sz, &id);
  if (rc != noErr) return kAudioDeviceUnknown;
  return id;
}

static std::shared_ptr<CoreAudioDeviceInfo>
_findByID(const AudioDeviceList& devlist, AudioDeviceID id, bool is_input) {
  if (id == kAudioDeviceUnknown) return nullptr;
  for (const auto& kv : devlist.GetMap()) {
    if (kv.second && kv.second->_ID == id && _matchesConstraints(kv.second, is_input)) {
      return kv.second;
    }
  }
  return nullptr;
}

static std::shared_ptr<CoreAudioDeviceInfo>
_firstMatching(const AudioDeviceList& devlist, bool is_input) {
  for (const auto& kv : devlist.GetMap()) {
    if (_matchesConstraints(kv.second, is_input)) {
      return kv.second;
    }
  }
  return nullptr;
}

// Tier 4 — display a native macOS dialog describing the missing hardware,
// then assert. osascript runs as a child; waitpid blocks the parent until
// the user dismisses, so the message is guaranteed to be seen before we
// crash. Falls through cleanly if osascript fails to launch (headless
// systems, SIP weirdness) — the assert still fires.
static void _showMissingAudioDialogAndAssert(bool input_missing, bool output_missing) {
  const char* which = (input_missing && output_missing) ? "input and output"
                    : input_missing                     ? "input"
                                                         : "output";
  char script[1024];
  snprintf(script, sizeof(script),
      "tell application \"System Events\" to activate\n"
      "display dialog \"No valid audio %s device found.\n\n"
      "Orkid requires a stereo-capable 48kHz output device and a "
      "1- or 2-channel 48kHz input device. Please connect suitable "
      "devices and ensure they are set to 48kHz using Audio/MIDI Setup, "
      "then relaunch.\" "
      "with icon stop buttons {\"OK\"} default button \"OK\" "
      "with title \"Orkid Audio\"",
      which);

  pid_t pid;
  char* const argv_buf[] = {
      (char*)"/usr/bin/osascript",
      (char*)"-e",
      script,
      NULL};
  int rc = posix_spawn(&pid, argv_buf[0], NULL, NULL, argv_buf, environ);
  if (rc == 0) {
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) { /* retry */ }
  } else {
    logerrchannel()->log("CoreAudio: posix_spawn(osascript) failed: rc=%d", rc);
  }
  OrkAssert(false && "no valid 48kHz audio device found");
}

} // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CoreAudioDevice::CoreAudioDevice(appinitdata_wkptr_t appinitd)
    : AudioDevice(appinitd)
    , _inputDevList(true)
    , _outputDevList(false) {

  auto unlocked_appinitdata = _appinitdata.lock();

  // Resolve short IDs (4-char hash) to full device names
  std::string input_devname = unlocked_appinitdata->_audio_input_devname;
  std::string output_devname = unlocked_appinitdata->_audio_output_devname;

  auto isShortId = [](const std::string& name) -> bool {
    if (name.length() != 4) return false;
    for (char c : name) {
      if (!std::isalnum(c)) return false;
    }
    return true;
  };

  if (isShortId(input_devname)) {
    auto dev = findAudioDeviceByShortId(input_devname);
    if (dev) {
      if(0)logchan_coreaudio->log("resolved input short id '%s' to '%s' @ %gHz",
                             input_devname.c_str(), dev->_name.c_str(), dev->_sample_rate);
      input_devname = dev->_name;
    } else {
      logerrchannel()->log("unknown input short id '%s' - run ork.devicelist.audio.py to see available IDs",
                           input_devname.c_str());
    }
  }
  if (isShortId(output_devname)) {
    auto dev = findAudioDeviceByShortId(output_devname);
    if (dev) {
      if(0)logchan_coreaudio->log("resolved output short id '%s' to '%s' @ %gHz",
                             output_devname.c_str(), dev->_name.c_str(), dev->_sample_rate);
      output_devname = dev->_name;
    } else {
      logerrchannel()->log("unknown output short id '%s' - run ork.devicelist.audio.py to see available IDs",
                           output_devname.c_str());
    }
  }

  // ----- INPUT: tier 1 → 2 → 3 -----
  if (unlocked_appinitdata->_enable_audio_input) {
    std::shared_ptr<CoreAudioDeviceInfo> info;
    // tier 1: env-requested name
    info = _findByName(_inputDevList, input_devname, /*is_input=*/true);
    // tier 2: CoreAudio system default
    if (!info) {
      AudioDeviceID sys_default = _getSystemDefaultID(/*is_input=*/true);
      info = _findByID(_inputDevList, sys_default, /*is_input=*/true);
      if (info) {
        logchan_coreaudio->log(
            "audio input: requested '%s' not available (not 48kHz-mono/stereo); using system default '%s'",
            input_devname.c_str(), info->_name.c_str());
      }
    }
    // tier 3: first-matching
    if (!info) {
      info = _firstMatching(_inputDevList, /*is_input=*/true);
      if (info) {
        logchan_coreaudio->log(
            "audio input: no match for env or system default; using first matching device '%s' (%dch, 48kHz)",
            info->_name.c_str(), info->countChannels());
      }
    }
    if (info) {
      _actual_input_channels = info->countChannels();
      _num_input_channels    = unlocked_appinitdata->_audio_input_numchannels;
      logchan_coreaudio->log(
          "FOUND INPUT DEVICE name<%s> device_ch<%d> requested_ch<%zu>",
          info->_name.c_str(), _actual_input_channels, _num_input_channels);
      _input_info = info;
    }
  }

  // ----- OUTPUT: tier 1 → 2 → 3 -----
  if (unlocked_appinitdata->_enable_audio_output) {
    std::shared_ptr<CoreAudioDeviceInfo> info;
    // tier 1: env-requested name
    info = _findByName(_outputDevList, output_devname, /*is_input=*/false);
    // tier 2: CoreAudio system default
    if (!info) {
      AudioDeviceID sys_default = _getSystemDefaultID(/*is_input=*/false);
      info = _findByID(_outputDevList, sys_default, /*is_input=*/false);
      if (info) {
        logchan_coreaudio->log(
            "audio output: requested '%s' not available (not 48kHz-stereo); using system default '%s'",
            output_devname.c_str(), info->_name.c_str());
      }
    }
    // tier 3: first-matching
    if (!info) {
      info = _firstMatching(_outputDevList, /*is_input=*/false);
      if (info) {
        logchan_coreaudio->log(
            "audio output: no match for env or system default; using first matching device '%s' (2ch, 48kHz)",
            info->_name.c_str());
      }
    }
    if (info) {
      logchan_coreaudio->log(
          "FOUND OUTPUT DEVICE name<%s> numch<%d>", info->_name.c_str(), info->countChannels());
      _output_info = info;
    }
  }

  // ----- tier 4: required direction missing → macOS dialog + assert -----
#if defined(DEBUG_NO_DEVICE_DIALOGUE)
  // Debug-only: force the "no device" path regardless of what tiers 1-3
  // found, so the macOS dialog can be exercised without unplugging audio
  // hardware. Clears both selections so the dialog text reads "input and
  // output" — flip one of these back to _input_info/_output_info if you
  // want to test the input-only or output-only variants.
  logerrchannel()->log("CoreAudio: DEBUG_NO_DEVICE_DIALOGUE active — forcing tier-4");
  _input_info.reset();
  _output_info.reset();
#endif
  bool input_missing  = unlocked_appinitdata->_enable_audio_input  && !_input_info;
  bool output_missing = unlocked_appinitdata->_enable_audio_output && !_output_info;
  if (input_missing || output_missing) {
    logerrchannel()->log(
        "CoreAudio: tier-4 bail — input_missing=%d output_missing=%d",
        int(input_missing), int(output_missing));
    _showMissingAudioDialogAndAssert(input_missing, output_missing);
  }
}

///////////////////////////////////////////////////////////////////////////////

void CoreAudioDevice::startup() {

  //logchan_coreaudio->log("CoreAudioDevice::startup");

  // Assert if called twice - audio should only be started once
  OrkAssert(_aucontext == nullptr && "CoreAudioDevice::startup() called twice!");

  auto unlocked_appinitdata = _appinitdata.lock();

  constexpr double desired_sample_rate = 48000.0;
  int inumfr                = desired_framesize;
  double seconds_per_buffer = static_cast<double>(inumfr) / desired_sample_rate;
  double available_time_us  = seconds_per_buffer * 1000000.0; // microseconds
                                                                        // For mach_time conversion
  mach_timebase_info_data_t timebase;
  mach_timebase_info(&timebase);
  const double nanos_to_micros = 1.0 / 1000.0;
  const double mach_to_nanos   = static_cast<double>(timebase.numer) / static_cast<double>(timebase.denom);

  float input_sample_rate  = 0.0f;
  float output_sample_rate = 0.0f;

  // Sample-rate is guaranteed to be 48 kHz by the tier-1/2/3 constraint filter
  // in the ctor — no need to re-assert here. If _input_info or _output_info is
  // null, the direction was either disabled or tier-4 would already have fired.
  if (_input_info) {
    _input_impl       = std::make_shared<CoreAudioDeviceImpl>(_input_info);
    input_sample_rate = _input_info->_format.mSampleRate;
  }
  if (_output_info) {
    _output_impl       = std::make_shared<CoreAudioDeviceImpl>(_output_info);
    output_sample_rate = _output_info->_format.mSampleRate;
  }

  _the_synth = synth::instance();
  _the_synth->setSampleRate(desired_sample_rate);
  _the_synth->waitUntilReady();
  _aucontext = std::make_shared<AuContext>();
  //logchan_coreaudio->log("CoreAudioThread _input_impl<%p> _output_impl<%p>", (void*)_input_impl.get(), (void*)_output_impl.get());

  if (_input_impl or _output_impl) {
    _aucontext->Init(_input_impl, _output_impl);
    _aucontext->Start();

    // Apply volume levels from environment variables (value in dB)
    auto _setDeviceVolumeDb = [](AudioDeviceID devID, bool isInput, float db) {
      AudioObjectPropertyAddress addr = {};
      addr.mSelector = kAudioDevicePropertyVolumeDecibels;
      addr.mScope    = isInput ? kAudioObjectPropertyScopeInput : kAudioObjectPropertyScopeOutput;
      // Try per-channel (1 and 2), then master element (0)
      for (UInt32 element : {1u, 2u, 0u}) {
        addr.mElement = element;
        if (AudioObjectHasProperty(devID, &addr)) {
          Boolean settable = false;
          if (AudioObjectIsPropertySettable(devID, &addr, &settable) == noErr && settable) {
            Float32 vol = db;
            AudioObjectSetPropertyData(devID, &addr, 0, NULL, sizeof(vol), &vol);
          }
        }
      }
    };

    if (_input_info) {
      if (auto env = std::getenv("ORKID_AUDIO_INPUT_LEVEL")) {
        float db = float(std::atof(env));
        _setDeviceVolumeDb(_input_info->_ID, true, db);
        logchan_coreaudio->log("set input volume for '%s' to %.1f dB", _input_info->_name.c_str(), db);
      }
    }
    if (_output_info) {
      if (auto env = std::getenv("ORKID_AUDIO_OUTPUT_LEVEL")) {
        float db = float(std::atof(env));
        _setDeviceVolumeDb(_output_info->_ID, false, db);
        logchan_coreaudio->log("set output volume for '%s' to %.1f dB", _output_info->_name.c_str(), db);
      }
    }

    _au_thread = std::make_shared<Thread>("CoreAudioThread");

    _noinputblock.resize(inumfr);
    for (int i = 0; i < inumfr; i++) {
      _noinputblock[i] = 0.0f; // interleaved
    }

    if (_output_impl) {
      if (!_aucontext->waitForOutputReady(2000)) {
        logerrchannel()->log(
            "CoreAudio: OUTPUT NEVER STARTED — device '%s' opened and AudioOutputUnitStart succeeded, but the HAL "
            "did not call _outputProc within 2000ms; startup aborts here and this app runs with NO audio. The unit "
            "is fine and the host stack is not: coreaudiod is wedged (it logs 'HALS_PlugIn: the object is not "
            "valid', and afplay fails -66681 for EVERY device including virtual ones) - restart it with "
            "'sudo killall coreaudiod'",
            _output_info ? _output_info->_name.c_str() : "<unknown>");
        return;
      }
    }

    // the synth learns its buffer geometry from the frame count it is handed,
    // and GROWING it walks all 512 pooled layers x 32 stage slots plus every
    // bus - ~18ms of allocation. left to the CoreAudioThread's first iteration
    // that lands with the HAL already pulling, so the output queue starves for
    // the whole of it. prime it here, on the caller's thread, with the count
    // that thread will use; a later resize stays on-demand.
    if (_the_synth) {
      _the_synth->resize(inumfr);
    }

    // [PA_DIAG] first-window telemetry (ORKID_PA_DIAG), the darwin twin of the
    // block in audiodevice_pa.cpp. SAME GRAMMAR ON PURPOSE: the committed
    // ratchet ork.lev2/pyext/tests/singularity/test_audio_first_window.py
    // parses both platforms with one set of regexes. What the numbers mean
    // here differs, because the synth does not run in the HAL IOProc on darwin
    // - it runs on this producer thread feeding _outputQueue - so a window
    // reports what THIS thread spent per buffer, against the same budget the
    // HAL drains at. faults_* are always 0 (darwin has no per-thread rusage);
    // underflows_total counts output-queue starvation, and the pre-roll
    // starves (the HAL runs before this thread exists) are reported separately
    // so they can never be read as drop-outs of a running stream.
    const bool diag_enabled = (getenv("ORKID_PA_DIAG") != nullptr);
    {
      auto& diagctrs = audioDiagCounters();
      diagctrs._frames_per_buffer.store(uint32_t(inumfr), std::memory_order_relaxed);
      diagctrs._sample_rate.store(float(desired_sample_rate), std::memory_order_relaxed);
    }

    _au_thread->start([=](anyp data) { //
        logchan_coreaudio->log("CoreAudioThread starting...");
      uint64_t diag_frames_window      = 0;
      uint64_t diag_max_ns_window      = 0;
      uint64_t diag_cpu_at_max_ns      = 0;
      uint64_t diag_underflows_reported = 0;
      while (_aucontext->_keep_going) {
        //printf("CoreAudioThread running\n");

        /////////////////////////
        // borrow StereoFragment (2 channels) from AuContext
        //   to hold output data
        /////////////////////////

        StereoFragment* mix_group = nullptr;
        if(_output_impl){
          mix_group = _aucontext->AllocOutBuffer(inumfr);
          // Check for shutdown - AllocOutBuffer returns nullptr during shutdown
          if (mix_group == nullptr) {
            //logchan_coreaudio->log("CoreAudioThread: got nullptr from AllocOutBuffer, exiting...");
            break;
          }
          //printf("got outbuf<%p>\n", (void*) mix_group);
          mix_group->Clear();

        }

        /////////////////////////
        // pull input data from device input queue
        //  via borrowed LayerFragment(n channels) from AuContext
        /////////////////////////

        LayerFragment* inpdata = nullptr;

        if(_input_impl){
          _aucontext->_inputQueue.try_pop(inpdata);

          /////////////////////////
          // invoke input handler if registered
          /////////////////////////

          if(inpdata and _input_handler) {
            // by convention,
            //  inputhandlers should NOT hold on to the AudioInputChunk
            auto chunk = std::make_shared<AudioInputChunk>(_num_input_channels);
            chunk->_num_frames = inumfr;
            chunk->_chunk_index++;
            OrkAssert(_num_input_channels >= 1);
            auto& chan0 = chunk->_channels[0];
            chan0.resize(inumfr);

            if (_actual_input_channels == 2 && _num_input_channels == 1) {
              // Mix stereo to mono
              const float* inL = (const float*) inpdata->mChannels[0].mSampleData;
              const float* inR = (const float*) inpdata->mChannels[1].mSampleData;
              for (size_t i = 0; i < inumfr; i++) {
                chan0[i] = (inL[i] + inR[i]) * 0.5f;
              }
            } else {
              // Mono or fallback: just take first channel
              const float* in = (const float*) inpdata->mChannels[0].mSampleData;
              for (size_t i = 0; i < inumfr; i++) {
                chan0[i] = in[i];
              }
            }

            _input_handler(chunk.get());
          }
        }

        /////////////////////////
        // run the synthesizer
        /////////////////////////

        if (_output_impl and _the_synth) {

          if (diag_enabled and (diag_frames_window == 0)) {
            printf("[PA_DIAG] framesPerBuffer<%lu> budget<%gus> SR<%g> backend<coreaudio>\n",
                   (unsigned long)inumfr, available_time_us, desired_sample_rate);
          }
          uint64_t diag_cpu0 = diag_enabled ? _threadCpuNanos() : 0;

          uint64_t start_time = mach_absolute_time();

          auto& outL             = mix_group->mMixLeft.mSampleData;
          auto& outR             = mix_group->mMixRight.mSampleData;
          float cpuload          = 0.0f;
          if(0){ // test tone
            static double phase = 0.0;
            for (size_t i = 0; i < inumfr; i++) {
              phase += 0.01f;;
              float samp   = sinf(phase) * 0.33f; // test
              outL[i] = samp;  // interleaved
              outR[i] = samp; // interleaved
            }
          }
          else{
            if (inpdata) {
              if (_actual_input_channels == 2 && _num_input_channels == 1) {
                // Mix stereo to mono for synth input
                float* inL = inpdata->mChannels[0].mSampleData;
                float* inR = inpdata->mChannels[1].mSampleData;
                for (size_t i = 0; i < inumfr; i++) {
                  _noinputblock[i] = (inL[i] + inR[i]) * 0.5f;
                }
                _the_synth->compute(inumfr, _noinputblock.data());
              } else {
                float* buffer = inpdata->mChannels[0].mSampleData;
                _the_synth->compute(inumfr, buffer);
              }
            } else {
              _the_synth->compute(inumfr, _noinputblock.data());
            }
            const auto& obuf = _the_synth->_obuf;
            for (size_t i = 0; i < inumfr; i++) {
              outL[i] = obuf._leftBuffer[i];  // interleaved
              outR[i] = obuf._rightBuffer[i]; // interleaved
            }
#if defined(DEBUG_LATENCY)
            // Latency probe at CoreAudio output (post-synthesizer)
            {
              using namespace ork::audio::singularity;
              static std::unique_ptr<TestPatternProbe> _ca_probe;
              static uint64_t _ca_probe_samples = 0;
              static double _ca_probe_sum = 0.0;
              static int _ca_probe_count = 0;
              static uint64_t _ca_probe_interval = 0;
              if (!_ca_probe) {
                float sr = _the_synth->_sampleRate;
                _ca_probe = std::make_unique<TestPatternProbe>(sr, 4096);
                _ca_probe->setChirpConfig(ChirpConfig());
                _ca_probe_interval = uint64_t(sr * 3.0);
              }
              _ca_probe->write(outL, inumfr);
              _ca_probe_samples += inumfr;
              if (_ca_probe->ready()) {
                double lat = _ca_probe->measureLatencyMs();
                if (lat >= 0.0) {
                  _ca_probe_sum += lat;
                  _ca_probe_count++;
                }
              }
              if (_ca_probe_interval > 0 && _ca_probe_samples >= _ca_probe_interval) {
                if (_ca_probe_count > 0) {
                  double avg = _ca_probe_sum / double(_ca_probe_count);
                  logchan_coreaudio->log("LATENCY PROBE (CoreAudio out): avg=%.1f ms (%d measurements)", avg, _ca_probe_count);
                }
                _ca_probe_samples = 0;
                _ca_probe_sum = 0.0;
                _ca_probe_count = 0;
              }
            }
#endif
          }
          uint64_t end_time     = mach_absolute_time();
          uint64_t elapsed_mach = end_time - start_time;
          double elapsed_nanos  = static_cast<double>(elapsed_mach) * mach_to_nanos;
          double elapsed_micros = elapsed_nanos * nanos_to_micros;

          // Calculate load as percentage of available time
          float buffer_cpu_load = static_cast<float>(elapsed_micros / available_time_us);

          // Update running average
          _cpu_load_accumulator.fetch_add(buffer_cpu_load);
          _cpu_load_sample_count.fetch_add(1);

          _the_synth->_cpuload = calculateCPULoad();
        static int counter = 0;
          if((counter%16)==0){
            logchan_coreaudio->perfItem("SYN.TIM(ms)", elapsed_micros*0.001f);
            logchan_coreaudio->perfItem("SYN.CPU(%)", _the_synth->_cpuload*100.0);
          }
          counter++;

          if (diag_enabled) {
            uint64_t diag_cpu_ns = _threadCpuNanos() - diag_cpu0;
            uint64_t diag_ns     = uint64_t(elapsed_nanos);
            if (diag_ns > diag_max_ns_window) {
              diag_max_ns_window = diag_ns;
              diag_cpu_at_max_ns = diag_cpu_ns;
            }
            diag_frames_window += inumfr;
            if (diag_frames_window >= uint64_t(desired_sample_rate * 5.0)) {
              auto& diagctrs = audioDiagCounters();
              uint64_t uf    = diagctrs._underflows.load(std::memory_order_relaxed);
              uint64_t pre   = _aucontext->_preroll_starves.load(std::memory_order_relaxed);
              printf("[PA_DIAG] window_max_compute<%gus> cpu_at_max<%gus> faults_at_max<%lu> faults_window<%lu> "
                     "budget<%gus> underflows_total<%lu> preroll_starves<%lu> backend<coreaudio>%s\n",
                     double(diag_max_ns_window) * 1e-3,
                     double(diag_cpu_at_max_ns) * 1e-3,
                     0ul,
                     0ul,
                     available_time_us,
                     (unsigned long)uf,
                     (unsigned long)pre,
                     (uf != diag_underflows_reported) ? " <<< NEW UNDERFLOWS" : "");
              fflush(stdout);
              diag_underflows_reported = uf;
              diag_frames_window       = 0;
              diag_max_ns_window       = 0;
              diag_cpu_at_max_ns       = 0;
            }
          }
        }

        /////////////////////////
        // return LayerFragment 
        //   to the AuContext
        /////////////////////////

        if (inpdata) {
          _aucontext->ReturnLayerFragment(inpdata);
        }

        /////////////////////////
        // push StereoFragment to output queue
        /////////////////////////
        if(mix_group){
          _aucontext->_outputQueue.push(mix_group);
        }

        /////////////////////////
      }
      logchan_coreaudio->log("CoreAudioThread exiting");
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

void CoreAudioDevice::shutdown() {
  // Stop audio thread FIRST so no callbacks can access synth during teardown
  if (_aucontext) {
    _aucontext->Stop();

    if (_au_thread) {
      _au_thread->join();
      _au_thread = nullptr;
    }

    _aucontext.reset();
  }

  // Now safe to tear down synth — no audio thread running
  if (_the_synth) {
    synth::tearDown();
  }

  logchan_coreaudio->log("CoreAudioDevice::shutdown() complete.");
}

///////////////////////////////////////////////////////////////////////////////

float CoreAudioDevice::calculateCPULoad() {
  int sample_count = _cpu_load_sample_count.load();
  if (sample_count >= CPU_LOAD_AVERAGE_SAMPLES) {
    float accumulated = _cpu_load_accumulator.exchange(0.0f);
    _cpu_load_sample_count.store(0);
    _last_cpu_load_reset = std::chrono::high_resolution_clock::now();
    return accumulated / static_cast<float>(sample_count);
  }
  return _the_synth ? _the_synth->_cpuload : 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

bool CoreAudioDeviceImpl::isValid() const {
  return _info->_ID != kAudioDeviceUnknown;
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioDeviceInfo::CoreAudioDeviceInfo(AudioDeviceID did, bool want_inputs) {
  _ID      = did;
  _isInput = want_inputs;
}

///////////////////////////////////////////////////////////////////////////////

CoreAudioDeviceImpl::CoreAudioDeviceImpl(coreaudio_device_info_ptr_t info) { //
  _info = info;
}

///////////////////////////////////////////////////////////////////////////////

void CoreAudioDeviceImpl::SetBufferSize(UInt32 size) {
  UInt32 propsize = sizeof(UInt32);
  __Verify_noErr(
      AudioDeviceSetProperty(_info->_ID, NULL, 0, _info->_isInput, kAudioDevicePropertyBufferFrameSize, propsize, &size));
  propsize = sizeof(UInt32);
  __Verify_noErr(AudioDeviceGetProperty(
      _info->_ID, 0, _info->_isInput, kAudioDevicePropertyBufferFrameSize, &propsize, &_info->_bufferSizeFrames));
}

///////////////////////////////////////////////////////////////////////////////

int CoreAudioDeviceInfo::countChannels() {
  OSStatus err;
  UInt32 propSize;
  int result = 0;

  err = AudioDeviceGetPropertyInfo(_ID, 0, _isInput, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
  if (err)
    return 0;

  AudioBufferList* buflist = (AudioBufferList*)malloc(propSize);
  err                      = AudioDeviceGetProperty(_ID, 0, _isInput, kAudioDevicePropertyStreamConfiguration, &propSize, buflist);
  if (!err) {
    for (UInt32 i = 0; i < buflist->mNumberBuffers; ++i) {
      result += buflist->mBuffers[i].mNumberChannels;
    }
  }
  free(buflist);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ca
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Enumeration function (in ork::lev2 namespace for linkage)
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

audiodeviceinfo_list_t enumerateAudioDevices_coreaudio() {
  audiodeviceinfo_list_t result;

  // Sample rates to probe
  static const double test_rates[] = {44100.0, 48000.0, 88200.0, 96000.0};
  static const int num_test_rates = sizeof(test_rates) / sizeof(test_rates[0]);

  // Get all audio devices
  UInt32 propsize;
  AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &propsize, NULL);
  int nDevices = propsize / sizeof(AudioDeviceID);
  AudioDeviceID* devids = new AudioDeviceID[nDevices];
  AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &propsize, devids);

  for (int i = 0; i < nDevices; ++i) {
    AudioDeviceID devid = devids[i];
    char name[256] = {0};
    UInt32 name_len = sizeof(name);

    // Get input channel count
    int input_channels = 0;
    UInt32 inPropSize;
    if (AudioDeviceGetPropertyInfo(devid, 0, true, kAudioDevicePropertyStreamConfiguration, &inPropSize, NULL) == noErr) {
      AudioBufferList* buflist = (AudioBufferList*)malloc(inPropSize);
      if (AudioDeviceGetProperty(devid, 0, true, kAudioDevicePropertyStreamConfiguration, &inPropSize, buflist) == noErr) {
        for (UInt32 b = 0; b < buflist->mNumberBuffers; ++b) {
          input_channels += buflist->mBuffers[b].mNumberChannels;
        }
      }
      free(buflist);
    }

    // Get output channel count
    int output_channels = 0;
    UInt32 outPropSize;
    if (AudioDeviceGetPropertyInfo(devid, 0, false, kAudioDevicePropertyStreamConfiguration, &outPropSize, NULL) == noErr) {
      AudioBufferList* buflist = (AudioBufferList*)malloc(outPropSize);
      if (AudioDeviceGetProperty(devid, 0, false, kAudioDevicePropertyStreamConfiguration, &outPropSize, buflist) == noErr) {
        for (UInt32 b = 0; b < buflist->mNumberBuffers; ++b) {
          output_channels += buflist->mBuffers[b].mNumberChannels;
        }
      }
      free(buflist);
    }

    // Skip devices with no audio channels
    if (input_channels == 0 && output_channels == 0) {
      continue;
    }

    // Get device name
    AudioDeviceGetProperty(devid, 0, false, kAudioDevicePropertyDeviceName, &name_len, name);

    // Get available sample rates
    std::vector<double> supported_rates;
    UInt32 range_size;
    if (AudioDeviceGetPropertyInfo(devid, 0, false, kAudioDevicePropertyAvailableNominalSampleRates, &range_size, NULL) == noErr) {
      int num_ranges = range_size / sizeof(AudioValueRange);
      AudioValueRange* ranges = (AudioValueRange*)malloc(range_size);
      if (AudioDeviceGetProperty(devid, 0, false, kAudioDevicePropertyAvailableNominalSampleRates, &range_size, ranges) == noErr) {
        for (int r = 0; r < num_test_rates; r++) {
          double rate = test_rates[r];
          for (int rng = 0; rng < num_ranges; rng++) {
            if (rate >= ranges[rng].mMinimum && rate <= ranges[rng].mMaximum) {
              supported_rates.push_back(rate);
              break;
            }
          }
        }
      }
      free(ranges);
    }

    // If no rates found, get current sample rate
    if (supported_rates.empty()) {
      AudioStreamBasicDescription format;
      UInt32 fmt_size = sizeof(format);
      if (AudioDeviceGetProperty(devid, 0, false, kAudioDevicePropertyStreamFormat, &fmt_size, &format) == noErr) {
        supported_rates.push_back(format.mSampleRate);
      }
    }

    // Create an entry for each supported sample rate
    for (double rate : supported_rates) {
      auto info = std::make_shared<AudioDeviceInfo>();
      info->_name = name;
      info->_device_index = i;
      info->_sample_rate = rate;
      info->_max_input_channels = input_channels;
      info->_max_output_channels = output_channels;
      info->_supported_input_rates = supported_rates;
      info->_supported_output_rates = supported_rates;
      result.push_back(info);
    }
  }

  delete[] devids;
  return result;
}

} // namespace ork::lev2

#endif // #if defined(ENABLE_CORE_AUDIO)
