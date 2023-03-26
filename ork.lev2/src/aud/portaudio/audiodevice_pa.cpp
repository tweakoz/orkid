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

#if defined(ENABLE_PORTAUDIO)

int WTF = 0;

using namespace ork::audio::singularity;

template class ork::orklut<ork::Char8, float>;

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
PaStream* pa_stream      = nullptr;
const bool ENABLE_OUTPUT = true; // allow disabling for long debug sessions
#if defined(__APPLE__)
const int DESIRED_NUMFRAMES = 256;
#else
const int DESIRED_NUMFRAMES = 1024;
#endif
///////////////////////////////////////////////////////////////////////////////

static int patestCallback(
    const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
  /* Cast data passed through stream to our structure. */
  float* out = (float*)outputBuffer;
  unsigned int i;
  (void)inputBuffer; /* Prevent unused variable warning. */

  synth_ptr_t the_synth = synth::instance();

  the_synth->compute(framesPerBuffer, inputBuffer);

  the_synth->_cpuload = Pa_GetStreamCpuLoad(pa_stream);

  if (false) { // test tone ?
    static int64_t _testtoneph = 0;
    for (int i = 0; i < framesPerBuffer; i++) {
      double phase = 440.0 * pi2 * double(_testtoneph) / getSampleRate();
      //printf( "phase<%g>\n", phase );
      float samp   = sinf(phase) * .6;
      *out++       = samp; // interleaved
      *out++       = samp; // interleaved
      _testtoneph++;
    }
  } else if (ENABLE_OUTPUT) {
    const auto& obuf = the_synth->_obuf;
    float gain       = the_synth->_masterGain;
    for (i = 0; i < framesPerBuffer; i++) {
      *out++ = obuf._leftBuffer[i]* gain;  // interleaved
      *out++ = obuf._rightBuffer[i]* gain; // interleaved
    }
  } else {
    for (i = 0; i < framesPerBuffer; i++) {
      *out++ = 0.0f; // interleaved
      *out++ = 0.0f; // interleaved
    }

  }
  return 0;
}

 static void _startupAudio() {

  synth::bringUp();
  
  synth_ptr_t the_synth = synth::instance();

  float SR = getSampleRate();

  the_synth->setSampleRate(SR);

  printf("SingularitySynth<%p> SR<%g>\n", (void*) the_synth.get(), SR);
  // loadPrograms();

  auto err = Pa_Initialize();
  OrkAssert(err == paNoError);

  /* Open an audio I/O stream. */
  err = Pa_OpenDefaultStream(
      &pa_stream,
      0,         // no input channels
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
      nullptr);          // This is a pointer that will be passed to
                         //         your callback

  OrkAssert(err == paNoError);

  err = Pa_StartStream(pa_stream);
  OrkAssert(err == paNoError);

  the_synth->resetFenables();
}

///////////////////////////////////////////////////////////////////////////////

void _tearDownAudio() {
  auto err = Pa_StopStream(pa_stream);
  assert(err == paNoError);
  err = Pa_Terminate();
  assert(err == paNoError);
  synth::tearDown();
}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePa::AudioDevicePa()
    : AudioDevice() {
    _startupAudio();
}

AudioDevicePa::~AudioDevicePa(){
  _tearDownAudio();
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
#endif 
