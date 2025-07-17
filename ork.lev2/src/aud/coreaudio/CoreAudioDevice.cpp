////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>
#if defined(ENABLE_CORE_AUDIO)

#include "CoreAudioDevice.h"
#include "au.h"
#include "ca_helpers/CARingBuffer.h"
#include "ca_helpers/CAStreamBasicDescription.h"
#include <libkern/OSAtomic.h>
#include <ork/util/logger.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <mach/mach_time.h>
#include <chrono>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::ca {
///////////////////////////////////////////////////////////////////////////////
using namespace ork::audio::singularity;

static logchannel_ptr_t logchan_coreaudio = logger()->configureChannel("PERF", fvec3(1, 0.6, .8), true);

void EnumerateMidiDevices() {
  int n                = MIDIGetNumberOfExternalDevices();
  MIDIEntityRef entity = NULL;

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
///////////////////////////////////////////////////////////////////////////////

CoreAudioDevice::CoreAudioDevice(appinitdata_wkptr_t appinitd)
    : AudioDevice(appinitd)
    , _inputDevList(true)
    , _outputDevList(false) {

  auto unlocked_appinitdata = _appinitdata.lock();

  if( unlocked_appinitdata->_enable_audio_input ) {
    for (const auto& input : _inputDevList.GetMap()) {
      auto info   = input.second;
      auto format = info->_format;

      auto fmtstr = CAStreamBasicDescription::Print(format);
      logchan_coreaudio->log(
          "input id<%d> name<%s> numchan<%d> fmt<%s>", info->_ID, input.first.c_str(), info->countChannels(), fmtstr.c_str());

      if (input.first == unlocked_appinitdata->_audio_input_devname) {
        //_inp_dev_name = input.first;
        _num_input_channels = info->countChannels();
        logchan_coreaudio->log("FOUND INPUT DEVICE !!!!! name<%s> numch<%d>", input.first.c_str(), info->countChannels());
        _input_info = info;
      }
    }
  }
  if( unlocked_appinitdata->_enable_audio_output ) {
    for (const auto& output : _outputDevList.GetMap()) {
      auto info   = output.second;
      auto format = info->_format;
      logchan_coreaudio->log("output id<%d> name<%s> numch<%d>", info->_ID, output.first.c_str(), info->countChannels());
      CAStreamBasicDescription::Print(format);
      if (output.first == _appinitdata.lock()->_audio_output_devname) {
        //_inp_dev_name = input.first;
        //_num_input_channels = info->countChannels();
        logchan_coreaudio->log("FOUND OUTPUT DEVICE !!!!! name<%s> numch<%d>", output.first.c_str(), info->countChannels());
        _output_info = info;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void CoreAudioDevice::startup() {

  auto unlocked_appinitdata = _appinitdata.lock();

  constexpr double desired_sample_rate = 48000.0;
  constexpr int inumfr                = desired_framesize;
  constexpr double seconds_per_buffer = static_cast<double>(inumfr) / desired_sample_rate;
  constexpr double available_time_us  = seconds_per_buffer * 1000000.0; // microseconds
                                                                        // For mach_time conversion
  mach_timebase_info_data_t timebase;
  mach_timebase_info(&timebase);
  const double nanos_to_micros = 1.0 / 1000.0;
  const double mach_to_nanos   = static_cast<double>(timebase.numer) / static_cast<double>(timebase.denom);

  float input_sample_rate  = 0.0f;
  float output_sample_rate = 0.0f;

  if (_input_info) {
    _input_impl       = std::make_shared<CoreAudioDeviceImpl>(_input_info);
    input_sample_rate = _input_info->_format.mSampleRate;
    OrkAssert(int(input_sample_rate) == int(desired_sample_rate));
  }
  if (_output_info) {
    _output_impl       = std::make_shared<CoreAudioDeviceImpl>(_output_info);
    output_sample_rate = _output_info->_format.mSampleRate;
    OrkAssert(int(output_sample_rate) == int(desired_sample_rate));
  }

  if (unlocked_appinitdata->_enable_audio_synth) {
    _the_synth = synth::instance();
    _the_synth->setSampleRate(desired_sample_rate);
  }

  _aucontext = std::make_shared<AuContext>();
  if (_input_impl or _output_impl) {
    _aucontext->Init(_input_impl, _output_impl);
    _aucontext->Start();

    _au_thread = std::make_shared<Thread>("CoreAudioThread");

    _noinputblock.resize(inumfr);
    for (int i = 0; i < inumfr; i++) {
      _noinputblock[i] = 0.0f; // interleaved
    }

    if (_output_impl) {
      if (!_aucontext->waitForOutputReady(2000)) {
        logchan_coreaudio->log("ERROR: Output callback not ready, aborting startup");
        return;
      }
    }

    _au_thread->start([=](anyp data) { //
      while (_aucontext->_keep_going) {
        //printf("CoreAudioThread running\n");

        /////////////////////////
        // borrow StereoFragment (2 channels) from AuContext
        //   to hold output data
        /////////////////////////

        StereoFragment* mix_group = nullptr;
        if(_output_impl){
          mix_group = _aucontext->AllocOutBuffer(inumfr);
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
            static auto chunk = std::make_shared<AudioInputChunk>(_num_input_channels);
            chunk->_num_frames = inumfr;
            chunk->_chunk_index++;
            OrkAssert(_num_input_channels >= 1);
            auto& chan0 = chunk->_channels[0];
            const float* in = (const float*) inpdata->mChannels[0].mSampleData;
            chan0.resize(inumfr);
            for (size_t i = 0; i < inumfr; i++) {
              chan0[i] = in[i];
            }
            _input_handler(chunk.get());
          }
        }

        /////////////////////////
        // run the synthesizer
        /////////////////////////

        if (_output_impl and _the_synth) {

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
              float* buffer = inpdata->mChannels[0].mSampleData;
              _the_synth->compute(inumfr, buffer);
            } else {
              _the_synth->compute(inumfr, _noinputblock.data());
            }
            const auto& obuf = _the_synth->_obuf;
            for (size_t i = 0; i < inumfr; i++) {
              outL[i] = obuf._leftBuffer[i];  // interleaved
              outR[i] = obuf._rightBuffer[i]; // interleaved
            }
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
            logchan_coreaudio->perfItem("SYNCPUTIM(ms)", elapsed_micros*0.001f);
            logchan_coreaudio->perfItem("SYNCPU(%)", _the_synth->_cpuload*100.0);
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
      OrkAssert(false);
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

void CoreAudioDevice::shutdown() {
  if (_the_synth) {
    synth::tearDown();
  }
  if (_aucontext) {
    _aucontext->Stop();
    _aucontext.reset();
  }
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

#endif // #if defined(ENABLE_CORE_AUDIO)
