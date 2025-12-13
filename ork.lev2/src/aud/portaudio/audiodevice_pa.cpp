////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include "audiodevice_pa.h"
#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/kernel/orklut.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <ork/application/application.h>
#include <portaudio.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <sstream>
#include <FLAC++/decoder.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/util/logger.h>

#if defined(ENABLE_PORTAUDIO)

using namespace ork::audio::singularity;

template class ork::orklut<ork::Char8, float>;

namespace ork::lev2 {
static logchannel_ptr_t logchan_portaudio = logger()->configureChannel("audio.PA", fvec3(1, 0.6, .8), true);

///////////////////////////////////////////////////////////////////////////////
PaStream* pa_stream      = nullptr;
const bool ENABLE_OUTPUT = true; // allow disabling for long debug sessions
#if defined(__APPLE__)
const int DESIRED_NUMFRAMES = 128;
#else
const int DESIRED_NUMFRAMES = 256;
#endif
///////////////////////////////////////////////////////////////////////////////

struct PaImpl {
  PaDeviceIndex _input_override = -1;
  PaDeviceIndex _output_override = -1;
  PaStream* _stream = nullptr;
  int _actual_input_channels = 0;  // actual device channel count (may differ from requested)
};

///////////////////////////////////////////////////////////////////////////////

static int patestCallback(
    const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {

  auto padev = (AudioDevicePa*) userData;

  auto paimpl = padev->_impl.getShared<PaImpl>();
  auto the_synth = padev->_the_synth;
  auto aid = padev->_appinitdata.lock();

  if(inputBuffer and padev->_input_handler){
    static auto chunk = std::make_shared<AudioInputChunk>(padev->_num_input_channels);
    chunk->_num_frames = framesPerBuffer;
    chunk->_chunk_index++;
    OrkAssert(padev->_num_input_channels >= 1);
    auto& chan0 = chunk->_channels[0];
    const int16_t* in = (const int16_t*)inputBuffer;
    chan0.resize(framesPerBuffer);

    constexpr float scale = 1.0f / 32768.0f;
    int actual_channels = paimpl->_actual_input_channels;

    if (actual_channels == 2 && padev->_num_input_channels == 1) {
      // Mix stereo to mono (int16 -> float)
      for (size_t i = 0; i < framesPerBuffer; i++) {
        float L = float(in[i * 2]) * scale;
        float R = float(in[i * 2 + 1]) * scale;
        chan0[i] = (L + R) * 0.5f;
      }
    } else if (actual_channels == 1) {
      // Mono input (int16 -> float)
      for (size_t i = 0; i < framesPerBuffer; i++) {
        chan0[i] = float(in[i]) * scale;
      }
    } else {
      // Fallback: just take first channel (int16 -> float)
      for (size_t i = 0; i < framesPerBuffer; i++) {
        chan0[i] = float(in[i * actual_channels]) * scale;
      }
    }
    padev->_input_handler(chunk.get());
  }

  if(the_synth and outputBuffer){
    auto out = (float*)outputBuffer;

    // convert inputBuffer to float
    static auto inputBufferFloat = new float[framesPerBuffer];
    const int16_t* inpbuf_int = nullptr;
    if(aid->_enable_audio_input and padev->_num_input_channels == 1){
      inpbuf_int = (const int16_t*)inputBuffer;
    }
    else{
      static std::vector<uint16_t> zeros(framesPerBuffer, 0);
      inpbuf_int = (const int16_t*)zeros.data(); // no input, fill with zeros
    }

    for (size_t i = 0; i < framesPerBuffer; i++) {
      int16_t j = inpbuf_int[i];
      // endian swap 
      ork::swapbytes(j);
      inputBufferFloat[i] = float(j) * (1.0f / 32768.0f); // convert to float
    }



    the_synth->compute(framesPerBuffer, inputBufferFloat);
    the_synth->_cpuload = Pa_GetStreamCpuLoad(pa_stream);

    if (false) { // test tone ?
      static int64_t _testtoneph = 0;
      for (size_t i = 0; i < framesPerBuffer; i++) {
        double phase = 440.0 * pi2 * double(_testtoneph) / getSampleRate();
        //printf( "phase<%g>\n", phase );
        float samp   = sinf(phase) * 1.0;
        *out++       = samp; // interleaved
        *out++       = samp; // interleaved
        _testtoneph++;
      }
    } else if (ENABLE_OUTPUT) {
      const auto& obuf = the_synth->_obuf;
      for (size_t i = 0; i < framesPerBuffer; i++) {
        *out++ = obuf._leftBuffer[i];  // interleaved
        *out++ = obuf._rightBuffer[i]; // interleaved
      }
    } else {
      for (size_t i = 0; i < framesPerBuffer; i++) {
        *out++ = 0.0f; // interleaved
        *out++ = 0.0f; // interleaved
      }

    }
  }
  else if(outputBuffer) { // no synth
    auto out = (float*)outputBuffer;
    for (size_t i = 0; i < framesPerBuffer; i++) {
      *out++ = 0.0f; // interleaved
      *out++ = 0.0f; // interleaved
    }
  }
  return 0;
}

 static void _startupAudio(AudioDevicePa* padev) {

  logchan_portaudio->log("starting audio");
  
  float SR = getSampleRate();

  if(padev->_the_synth){
    padev->_the_synth->setSampleRate(SR);
    printf("SingularitySynth<%p> SR<%g>\n", (void*) padev->_the_synth.get(), SR);
    // loadPrograms();
  }

  auto paimpl = padev->_impl.makeShared<PaImpl>();
  auto err = Pa_Initialize();
  OrkAssert(err == paNoError);
  int num_inputs = 0;
  int num_outputs = 0;
  auto aid = padev->_appinitdata.lock();
  if( aid->_enable_audio_input ) {
    num_inputs = padev->_num_input_channels;
  }
  if( aid->_enable_audio_output ) {
    num_outputs = padev->_num_output_channels;
  }

  logchan_portaudio->log("desired input device name<%s>", padev->_inp_dev_name.c_str());
  logchan_portaudio->log("desired output device name<%s>", padev->_out_dev_name.c_str());
  logchan_portaudio->log("req num_inp<%zu> num_out<%zu>", num_inputs, num_outputs);

  size_t num_devices = Pa_GetDeviceCount();
  logchan_portaudio->log("num devices<%zu>", num_devices);

  bool input_default = padev->_inp_dev_name == "default";
  bool output_default = padev->_out_dev_name == "default";
  bool got_input = false;
  bool got_output = false;


  logchan_portaudio->log("input_default<%s> output_default<%s>",
                         input_default ? "true" : "false",
                         output_default ? "true" : "false");

  for(size_t c=0; c<num_devices; c++){
    auto devinfo = Pa_GetDeviceInfo(c);
    std::string devname = devinfo->name;
    size_t num_inp = devinfo->maxInputChannels;
    size_t num_out = devinfo->maxOutputChannels;
    logchan_portaudio->log("device<%zu> name<%s> num_inp<%zu> num_out<%zu>", c, devinfo->name, num_inp, num_out);


    if( (num_inputs>0) and (num_inp >= num_inputs) and paimpl->_input_override == -1 ){
      bool substr_matched = (devname.find(padev->_inp_dev_name)==0);
      if(substr_matched or input_default){
        logchan_portaudio->log("using device<%s> for input (device has %zu channels, requested %d)",
                               devname.c_str(), num_inp, num_inputs);
        paimpl->_input_override = c;
        paimpl->_actual_input_channels = num_inp;
        got_input = true;
      }
    }
    if( (num_outputs>0) and (num_out == num_outputs) and paimpl->_output_override == -1 ){
      bool substr_matched = (devname.find(padev->_out_dev_name)==0);
      if(substr_matched or output_default){
        logchan_portaudio->log("using device<%s> for output", devname.c_str());
        paimpl->_output_override = c;
        got_output = true;
      }
    }
  }

  PaStreamParameters inp_params, out_params;
  if(num_inputs>0){
    if(not got_input){
      logerrchannel()->log("could not open input device<%s>", padev->_inp_dev_name.c_str());
      OrkAssert(false);
    }
    inp_params.device = paimpl->_input_override;
    inp_params.channelCount = paimpl->_actual_input_channels;  // use actual device channels
    inp_params.sampleFormat = paInt16;
    inp_params.suggestedLatency = Pa_GetDeviceInfo(inp_params.device)->defaultLowInputLatency;
    inp_params.hostApiSpecificStreamInfo = nullptr;
    logchan_portaudio->log("opening input with %d channels (will mix to %d)",
                           paimpl->_actual_input_channels, num_inputs);
  }

  if(num_outputs>0){
    if(not got_output){
      logerrchannel()->log("could not open output device<%s>", padev->_inp_dev_name.c_str());
      OrkAssert(false);
    }
    out_params.device = paimpl->_output_override;
    out_params.channelCount = num_outputs;
    out_params.sampleFormat = paFloat32;
    out_params.suggestedLatency = Pa_GetDeviceInfo(out_params.device)->defaultLowOutputLatency;
    out_params.hostApiSpecificStreamInfo = nullptr;
  }

  if( (num_inputs>0) and (num_outputs>0) ){
    OrkAssert(got_input);
    err = Pa_OpenStream(
        &pa_stream,
        &inp_params,
        &out_params,
        SR,
        DESIRED_NUMFRAMES,
        paClipOff|paDitherOff,
        patestCallback,
        (void*) padev );
    OrkAssert(err == paNoError);
  }
  else if( (num_outputs>0) ){
    OrkAssert(got_output);
    err = Pa_OpenStream(
        &pa_stream,
        nullptr,
        &out_params,
        SR,
        DESIRED_NUMFRAMES,
        paClipOff,
        patestCallback,
        (void*) padev );
    OrkAssert(err == paNoError);
  }
  else if( (num_inputs>0) ){
    OrkAssert(got_input);
    err = Pa_OpenStream(
        &pa_stream,
        &inp_params,
        nullptr,
        SR,
        DESIRED_NUMFRAMES,
        paClipOff,
        patestCallback,
        (void*) padev );
    OrkAssert(err == paNoError);
  }
  else{
    OrkAssert(false);
  }

  err = Pa_StartStream(pa_stream);
  OrkAssert(err == paNoError);

  logchan_portaudio->log("have default stream<%p>", pa_stream);

  if(padev->_the_synth){
    padev->_the_synth->resetFenables();
  }
}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePa::AudioDevicePa(appinitdata_wkptr_t appinitd)
    : AudioDevice(appinitd) {
  
  
  /////////////////////////////////////

  _num_input_channels = 0;
  if(appinitd.lock()->_enable_audio_input){
    _num_input_channels = _appinitdata.lock()->_audio_input_numchannels;
  }
  _num_output_channels = _appinitdata.lock()->_audio_output_numchannels;

  _inp_dev_name = _appinitdata.lock()->_audio_input_devname;
  _out_dev_name = _appinitdata.lock()->_audio_output_devname;

}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePa::~AudioDevicePa(){
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::startup(){
  if(_appinitdata.lock()->_enable_audio_synth){
    _the_synth = synth::instance();
  }
  _startupAudio(this);

}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::shutdown(){
  logchan_portaudio->log("tearing down audio");
  auto err = Pa_StopStream(pa_stream);
  OrkAssert(err == paNoError);
  err = Pa_Terminate();
  OrkAssert(err == paNoError);
  if(_the_synth){
    synth::tearDown();
  }
}

///////////////////////////////////////////////////////////////////////////////

audiodeviceinfo_list_t enumerateAudioDevices_portaudio() {
  audiodeviceinfo_list_t result;
  Pa_Initialize();
  int num_devices = Pa_GetDeviceCount();
  for (int i = 0; i < num_devices; i++) {
    auto pa_info = Pa_GetDeviceInfo(i);
    auto info = std::make_shared<AudioDeviceInfo>();
    info->_name = pa_info->name;
    info->_max_input_channels = pa_info->maxInputChannels;
    info->_max_output_channels = pa_info->maxOutputChannels;
    info->_default_sample_rate = pa_info->defaultSampleRate;
    result.push_back(info);
  }
  Pa_Terminate();
  return result;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
#endif 
