////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>

#if defined(ENABLE_ALSA)

#include <ork/pch.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <alsa/asoundlib.h>
#include "audiodevice_alsa.h"
#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/kernel/orklut.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <ork/application/application.h>
#include <FLAC++/decoder.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/util/multi_buffer.h>


using namespace ork::audio::singularity;

//#define PCM_DEVICE "sysdefault:CARD=Pro"
#define PCM_DEVICE "default"

namespace ork::lev2 {

static constexpr int DESIRED_NUMFRAMES = 256;
static constexpr int KBUFFERCOUNT = 4;

static synth_ptr_t the_synth = synth::instance();

static float* _float_buf = nullptr;
snd_pcm_uframes_t _numframes = 0;

struct BUFFER{
  BUFFER(int numfr, int numch){_s16_buf=new int16_t[numfr*numch];}
  ~BUFFER() { delete[] _s16_buf; }
  int16_t* _s16_buf = nullptr;
};
using buffer_t = std::shared_ptr<BUFFER>;

ork::MpMcBoundedQueue<buffer_t, KBUFFERCOUNT> _multibufProducer;
ork::MpMcBoundedQueue<buffer_t, KBUFFERCOUNT> _multibufConsumer;

static void _startupAudio() {

  float SR = getSampleRate();
  the_synth->setSampleRate(SR);

  unsigned int pcm, tmp, dir;
  unsigned int rate     = int(SR);
  unsigned int channels = 2;
  snd_pcm_t* pcm_handle;
  snd_pcm_hw_params_t* params;

  // Open the PCM device in playback mode
  if ((pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    printf("ERROR: Can't open \"%s\" PCM device. %s\n", PCM_DEVICE, snd_strerror(pcm));

  // Allocate parameters object and fill it with default values
  snd_pcm_hw_params_alloca(&params);

  snd_pcm_hw_params_any(pcm_handle, params);

  // Set parameters
  if ((pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

  if ((pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE)) < 0)
    printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));

  if ((pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels)) < 0)
    printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

  if ((pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0)) < 0)
    printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

  /* Write parameters */
  if ((pcm = snd_pcm_hw_params(pcm_handle, params)) < 0)
    printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));

  printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));
  printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

  snd_pcm_hw_params_get_channels(params, &tmp);
  snd_pcm_hw_params_get_rate(params, &tmp, 0);
  _numframes = DESIRED_NUMFRAMES;
  snd_pcm_hw_params_set_period_size(pcm_handle,params,_numframes,0);

  _float_buf = new float[_numframes * channels];

  //////////////////////////////////////////////////////////////////////////
  // allocate int16_t buffers
  //////////////////////////////////////////////////////////////////////////

  for( int i=0; i<KBUFFERCOUNT; i++ )
    _multibufProducer.push(std::make_shared<BUFFER>(_numframes,channels));

  //////////////////////////////////////////////////////////////////////////

  snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

  buffer_t popped;
  while (true) {
    if(_multibufConsumer.try_pop(popped)) {
      if ((pcm = snd_pcm_writei(pcm_handle, popped->_s16_buf, _numframes)) == -EPIPE) {
        printf("XRUN.\n");
        snd_pcm_prepare(pcm_handle);
      } else if (pcm < 0) {
        printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
      } else {
      }
      _multibufProducer.push(popped);
    }
  }

  snd_pcm_drain(pcm_handle);
  snd_pcm_close(pcm_handle);
}

///////////////////////////////////////////////////////////////////////////////

AudioDeviceAlsa::AudioDeviceAlsa()
    : AudioDevice() {

  _alsaThread.start([=](anyp data) {
    _startupAudio();
  });

  _synthThread.start([=](anyp data) {
    buffer_t popped;
    while (true) {
      if (_multibufProducer.try_pop(popped)) {
        the_synth->compute(_numframes, _float_buf);
        const auto& obuf = the_synth->_obuf;
        float gain       = the_synth->_masterGain;
        auto sbuf = popped->_s16_buf;
        auto lbuf =  obuf._leftBuffer;
        auto rbuf =  obuf._rightBuffer;
        float gint = gain*16384.0f;
        for (size_t i = 0; i < _numframes; i++) {
          sbuf[i * 2 + 0] = int16_t(lbuf[i] * gint);  // interleaved
          sbuf[i * 2 + 1] = int16_t(rbuf[i] * gint); // interleaved
        }
        _multibufConsumer.push(popped);
      }
    }
    });


}

} // namespace ork::lev2 {
#endif
  