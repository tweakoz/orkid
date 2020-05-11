////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>

using namespace ork::audio::singularity;

template class ork::orklut<ork::Char8, float>;

namespace ork::lev2 {

PaStream* pa_stream = nullptr;

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

synth* the_synth = nullptr;

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

  the_synth->compute(framesPerBuffer, inputBuffer);
  const auto& obuf = the_synth->_obuf;

  for (i = 0; i < framesPerBuffer; i++) {
    *out++ = obuf._leftBuffer[i];
    *out++ = obuf._rightBuffer[i];
  }
  return 0;
}

void startupAudio() {
  assert(the_synth == nullptr);

  float SR = getSampleRate();

  the_synth              = new synth(SR);
  the_synth->_masterGain = 10.0f;

  printf("SingularitySynth<%p> SR<%g>\n", the_synth, SR);
  // loadPrograms();

  auto err = Pa_Initialize();
  OrkAssert(err == paNoError);

  /* Open an audio I/O stream. */
  err = Pa_OpenDefaultStream(
      &pa_stream,
      0,         // no input channels
      2,         // stereo output
      paFloat32, // 32 bit floating point output
      the_synth->_sampleRate,
      256,            /* frames per buffer, i.e. the number
                             of sample frames that PortAudio will
                             request from the callback. Many apps
                             may want to use
                             paFramesPerBufferUnspecified, which
                             tells PortAudio to pick the best,
                             possibly changing, buffer size.*/
      patestCallback, // this is your callback function
      nullptr);       // This is a pointer that will be passed to
                      //         your callback

  OrkAssert(err == paNoError);

  err = Pa_StartStream(pa_stream);
  OrkAssert(err == paNoError);

  the_synth->resetFenables();
}

///////////////////////////////////////////////////////////////////////////////

void tearDownAudio() {
  auto err = Pa_StopStream(pa_stream);
  assert(err == paNoError);
  err = Pa_Terminate();
  assert(err == paNoError);
}

///////////////////////////////////////////////////////////////////////////////

static const float flength = 1.0f;

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::DoReInitDevice(void) {
}

///////////////////////////////////////////////////////////////////////////////

AudioDevicePa::AudioDevicePa()
    : AudioDevice()
    , mHandles() {
  startupAudio();
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::Update(float fdt) {
  const HandlePool::pointervect_type& used = mHandles.used();

  fixedvector<AudioStreamPlayback*, 8> killed;

  for (HandlePool::pointervect_type::const_iterator it = used.begin(); it != used.end(); it++) {
    AudioStreamPlayback* pb = (*it);
    PaPlayHandle* phandle   = (PaPlayHandle*)pb->mpPlatformHandle;
    phandle->Update(fdt);

    PaStreamData* psd = phandle->mpstreamdata;

    if (psd) {
      if (phandle->fstrtime > psd->mfstreamlen) {
        killed.push_back(pb);
      }
    }
  }

  for (fixedvector<AudioStreamPlayback*, 8>::const_iterator it = killed.begin(); it != killed.end(); it++) {
    AudioStreamPlayback* handle = (*it);
    mHandles.deallocate(handle);
  }
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::DoInitSample(AudioSample& sample) {
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::SetPauseState(bool bpause) {
  return;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::DoPlaySound(AudioInstrumentPlayback* PlaybackHandle) {
  return;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::DoStopSound(AudioInstrumentPlayback* PlaybackHandle) {
}

///////////////////////////////////////////////////////////////////////////////

bool AudioDevicePa::DoLoadStream(AudioStream* streamhandle, ConstString fname) {
  AssetPath filename = fname.c_str();

  bool b_uncompressed = (strstr(filename.c_str(), "streams") != 0);

  bool b_looped = (strstr(filename.c_str(), "Music") != 0) | (strstr(filename.c_str(), "stings") != 0);
  b_looped |= (strstr(filename.c_str(), "music") != 0) | (strstr(filename.c_str(), "Stings") != 0);

  PaStreamData* psdata = new PaStreamData;

  float fmaxtime = 0.0f;

  streamhandle->SetPlatformHandle(psdata);

  if (false == b_uncompressed) {
    ork::EndianContext ec;
    ec.mendian = EENDIAN_LITTLE;

    filename.SetExtension("mkr");

    File ifile(filename, ork::EFM_READ);

    int icount = 0;
    ifile.Read(&icount, sizeof(icount));
    swapbytes_dynamic(icount);

    psdata->stream_markers.reserve(icount);

    for (int i = 0; i < icount; i++) {
      float ftime = 0.0f;
      int istrlen = 0;

      ifile.Read(&ftime, sizeof(ftime));
      ifile.Read(&istrlen, sizeof(istrlen));
      swapbytes_dynamic(ftime);
      swapbytes_dynamic(istrlen);

      char* pstring = new char[istrlen + 1];
      memset(pstring, 0, istrlen + 1);

      ifile.Read(pstring, istrlen);

      orkprintf("StreamMarker<%d> time<%f> name<%s>\n", i, ftime, pstring);

      // FMOD_SYNCPOINT *syncpoint = 0;

      unsigned int offset = int(ftime * 1000.0f);
      // FMOD_TIMEUNIT offsettype = FMOD_TIMEUNIT_MS;
      const char* name = pstring;
      // FMOD_SYNCPOINT **  point
      // fmod_result = phandle->addSyncPoint( offset, offsettype, name, & syncpoint );

      if (ftime > fmaxtime) {
        fmaxtime = ftime;
      }
      psdata->stream_markers.insert(std::pair<Char8, float>(name, ftime));
      // OrkAssert( fmod_result == FMOD_OK );
    }
    ifile.Close();
  }
  psdata->mfstreamlen = fmaxtime + 3.0f;
  psdata->stream_markers.insert(std::pair<Char8, float>("END", psdata->mfstreamlen));

  return true;
}

///////////////////////////////////////////////////////////////////////////////

AudioStreamPlayback* AudioDevicePa::DoPlayStream(AudioStream* streamhandle) {
  AudioStreamPlayback* pb = mHandles.allocate();
  PaPlayHandle* phandle   = (PaPlayHandle*)pb->mpPlatformHandle;

  phandle->Init();

  phandle->mpstreamdata = (PaStreamData*)streamhandle->GetPlatformHandle();

  return pb;
}

///////////////////////////////////////////////////////////////////////////////

float AudioDevicePa::GetStreamTime(AudioStreamPlayback* streampb_handle) {
  PaPlayHandle* phandle = (PaPlayHandle*)streampb_handle;
  return phandle->fstrtime;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::SetStreamTime(AudioStreamPlayback* streampb_handle, float ftime) {
  PaPlayHandle* phandle = (PaPlayHandle*)streampb_handle;
  if (phandle) {
    phandle->fstrtime = ftime;
  }
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::DoStopStream(AudioStreamPlayback* pb) {
  PaPlayHandle* phandle = (PaPlayHandle*)pb->mpPlatformHandle;

  const HandlePool::pointervect_type& used = mHandles.used();
  fixedvector<AudioStreamPlayback*, 8> killed;

  for (HandlePool::pointervect_type::const_iterator it = used.begin(); it != used.end(); it++) {
    AudioStreamPlayback* ptest = (*it);

    if (ptest == pb) {
      killed.push_back(ptest);
    }
  }

  for (fixedvector<AudioStreamPlayback*, 8>::const_iterator it = killed.begin(); it != killed.end(); it++) {
    AudioStreamPlayback* pb = (*it);
    PaPlayHandle* phandle   = (PaPlayHandle*)pb->mpPlatformHandle;

    phandle->mpstreamdata = (PaStreamData*)0;
    mHandles.deallocate(pb);
  }
}

float AudioDevicePa::GetStreamPlaybackLength(AudioStreamPlayback* streampb_handle) {
  if (streampb_handle) {
    PaPlayHandle* pnph = (PaPlayHandle*)streampb_handle;
    PaStreamData* psd  = pnph->mpstreamdata;

    if (psd) {
      return psd->mfstreamlen;
    }
  }
  return 5.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::SetStreamVolume(AudioStreamPlayback* streampbh, float fvol) {
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevicePa::FillInSyncPoints(AudioStream* streamhandle, orkmap<ork::PoolString, float>& points) {
  PaStreamData* psd = (PaStreamData*)streamhandle->GetPlatformHandle();

  if (psd) {
    for (orklut<Char8, float>::const_iterator it = psd->stream_markers.begin(); it != psd->stream_markers.end(); it++) {
      float fv  = it->second;
      Char8 ch8 = it->first;
      points.insert(std::make_pair(ork::AddPooledString(ch8.c_str()), fv));
    }
  }
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
