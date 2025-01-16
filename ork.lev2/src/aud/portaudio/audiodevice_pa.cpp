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
static logchannel_ptr_t logchan_portaudio = logger()->createChannel("audio.PA", fvec3(1, 0.6, .8), true);

///////////////////////////////////////////////////////////////////////////////
PaStream* pa_stream      = nullptr;
const bool ENABLE_OUTPUT = true; // allow disabling for long debug sessions
#if defined(__APPLE__)
const int DESIRED_NUMFRAMES = 128;
#else
const int DESIRED_NUMFRAMES = 256;
#endif
///////////////////////////////////////////////////////////////////////////////

static int patestCallback(
    const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {

  auto padev = (AudioDevicePa*) userData;
  auto the_synth = padev->_the_synth;


  /* Cast data passed through stream to our structure. */
  float* out = (float*)outputBuffer;
  unsigned int i;
 
  if(inputBuffer and padev->_input_handler){
    static auto chunk = std::make_shared<AudioInputChunk>(padev->_num_input_channels);
    chunk->_num_frames = framesPerBuffer;
    chunk->_chunk_index++;
    OrkAssert(padev->_num_input_channels == 1);
    auto& chan0 = chunk->_channels[0];
    const float* in = (const float*)inputBuffer;
    chan0.resize(framesPerBuffer);
    for (i = 0; i < framesPerBuffer; i++) {
      chan0[i] = in[i];
    }
    padev->_input_handler(chunk.get());
  }

  if(the_synth){
    the_synth->compute(framesPerBuffer, inputBuffer);
    the_synth->_cpuload = Pa_GetStreamCpuLoad(pa_stream);

    if (false) { // test tone ?
      static int64_t _testtoneph = 0;
      for (int i = 0; i < framesPerBuffer; i++) {
        double phase = 440.0 * pi2 * double(_testtoneph) / getSampleRate();
        //printf( "phase<%g>\n", phase );
        float samp   = sinf(phase) * 1.0;
        *out++       = samp; // interleaved
        *out++       = samp; // interleaved
        _testtoneph++;
      }
    } else if (ENABLE_OUTPUT) {
      const auto& obuf = the_synth->_obuf;
      for (i = 0; i < framesPerBuffer; i++) {
        *out++ = obuf._leftBuffer[i];  // interleaved
        *out++ = obuf._rightBuffer[i]; // interleaved
      }
    } else {
      for (i = 0; i < framesPerBuffer; i++) {
        *out++ = 0.0f; // interleaved
        *out++ = 0.0f; // interleaved
      }

    }
  }
  else { // no synth
    for (i = 0; i < framesPerBuffer; i++) {
      *out++ = 0.0f; // interleaved
      *out++ = 0.0f; // interleaved
    }
  }
  return 0;
}

 static void _startupAudio(AudioDevicePa* dev) {

  logchan_portaudio->log("starting audio");
  
  float SR = getSampleRate();

  if(dev->_the_synth){
    dev->_the_synth->setSampleRate(SR);
    printf("SingularitySynth<%p> SR<%g>\n", (void*) dev->_the_synth.get(), SR);
    // loadPrograms();
  }

  auto err = Pa_Initialize();
  OrkAssert(err == paNoError);
  int num_inputs = 0;
  auto aid = dev->_appinitdata.lock();
  if( aid->_enable_audio_input )
    num_inputs = 1;

  /* Open an audio I/O stream. */
  err = Pa_OpenDefaultStream(
      &pa_stream,
      num_inputs,// num input channels
      2,         // stereo output
      paFloat32, // 32 bit floating point output
      SR,
      DESIRED_NUMFRAMES, /* frames per buffer, i.e. the number
                  of sample frames that PortAudio will
                  request from the callback. Many apps
                  may want to use
                  paFramesPerBufferUnspecified, which
                  tells PortAudio to pick the best,
                  possibly changing, buffer size.*/
      patestCallback,    // this is your callback function
      (void*) dev );     // user pointer

  OrkAssert(err == paNoError);

  err = Pa_StartStream(pa_stream);
  OrkAssert(err == paNoError);

  logchan_portaudio->log("have default stream<%p>", pa_stream);

  if(dev->_the_synth){
    dev->_the_synth->resetFenables();
  }
}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePa::AudioDevicePa(appinitdata_wkptr_t appinitd)
    : AudioDevice(appinitd) {

  /////////////////////////////////////

  _num_input_channels = 0;
  if(appinitd.lock()->_enable_audio_input){
    _num_input_channels = 1;
  }
  _num_output_channels = 2;

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

} // namespace ork::lev2
#endif 
