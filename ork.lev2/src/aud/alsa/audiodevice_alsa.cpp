////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/util/multi_buffer.h>
#include <set>
#include <algorithm>

using namespace ork::audio::singularity;

//#define PCM_DEVICE "sysdefault:CARD=Pro"
#define PCM_DEVICE "default"

namespace ork::lev2 {

///////////////////////////////////////////////////////
static constexpr int DESIRED_NUMFRAMES = 256;
static constexpr int KBUFFERCOUNT      = 4;
struct BUFFER {
  BUFFER(int numfr, int numch) {
    _s16_buf = new int16_t[numfr * numch];
  }
  ~BUFFER() {
    delete[] _s16_buf;
  }
  int16_t* _s16_buf = nullptr;
};
using buffer_t = std::shared_ptr<BUFFER>;

///////////////////////////////////////////////////////

struct PrivateImplementation {

  PrivateImplementation(appinitdata_wkptr_t appinitd);
  ~PrivateImplementation();
  synth_ptr_t _synth;
  ork::Thread _alsaThread;
  ork::Thread _synthThread;
  float* _float_buf     = nullptr;
  snd_pcm_uframes_t _numframes = 0;
  ork::MpMcBoundedQueue<buffer_t, KBUFFERCOUNT> _multibufProducer;
  ork::MpMcBoundedQueue<buffer_t, KBUFFERCOUNT> _multibufConsumer;
  std::atomic<int> _execstate;
};

using impl_ptr_t = std::shared_ptr<PrivateImplementation>;

///////////////////////////////////////////////////////

PrivateImplementation::PrivateImplementation(appinitdata_wkptr_t appinitd) {
  _execstate.store(0);
  _synth = synth::instance();
  _synth->waitUntilReady();

  _alsaThread.start([=](anyp data) {
    float SR = getSampleRate();
    _synth->setSampleRate(SR);

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
    snd_pcm_hw_params_set_period_size(pcm_handle, params, _numframes, 0);

    _float_buf = new float[_numframes * channels];

    //////////////////////////////////////////////////////////////////////////
    // allocate int16_t buffers
    //////////////////////////////////////////////////////////////////////////

    for (int i = 0; i < KBUFFERCOUNT; i++)
      _multibufProducer.push(std::make_shared<BUFFER>(_numframes, channels));

    //////////////////////////////////////////////////////////////////////////

    snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

    buffer_t popped;
    _execstate.store(1);
    while (_execstate.load()==1) {
      if (_multibufConsumer.try_pop(popped)) {
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
    _execstate.store(3);

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
  });

  _synthThread.start([=](anyp data) {
    while (_execstate.load()!=1) {
      ork::usleep(1000);
    }
    buffer_t popped;
    while (_execstate.load()==1) {
      if (_multibufProducer.try_pop(popped)) {
        _synth->compute(_numframes, _float_buf);
        const auto& obuf = _synth->_obuf;
        auto sbuf        = popped->_s16_buf;
        auto lbuf        = obuf._leftBuffer;
        auto rbuf        = obuf._rightBuffer;
        float gint       = 16384.0f;
        for (size_t i = 0; i < _numframes; i++) {
          sbuf[i * 2 + 0] = int16_t(lbuf[i] * gint); // interleaved
          sbuf[i * 2 + 1] = int16_t(rbuf[i] * gint); // interleaved
        }
        _multibufConsumer.push(popped);
      }
    }
  });
}
PrivateImplementation::~PrivateImplementation() {
  _execstate.store(2);
  while (_execstate.load()==2) {
    ork::usleep(1000);
  }
  synth::tearDown();
}
///////////////////////////////////////////////////////////////////////////////

AudioDeviceAlsa::AudioDeviceAlsa(appinitdata_wkptr_t appinitd)
    : AudioDevice(appinitd) {

  _impl.makeShared<PrivateImplementation>(appinitd);
}

///////////////////////////////////////////////////////////////////////////////

// Helper to probe supported sample rates for an ALSA device
static std::vector<double> probeAlsaSampleRates(const char* device_name, bool is_capture) {
  std::vector<double> supported_rates;
  static const unsigned int test_rates[] = {44100, 48000, 88200, 96000};
  static const int num_test_rates = sizeof(test_rates) / sizeof(test_rates[0]);

  snd_pcm_t* pcm = nullptr;
  snd_pcm_hw_params_t* params = nullptr;

  int err = snd_pcm_open(&pcm, device_name,
                         is_capture ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                         SND_PCM_NONBLOCK);
  if (err < 0) {
    return supported_rates;
  }

  snd_pcm_hw_params_alloca(&params);
  if (snd_pcm_hw_params_any(pcm, params) >= 0) {
    for (int i = 0; i < num_test_rates; i++) {
      if (snd_pcm_hw_params_test_rate(pcm, params, test_rates[i], 0) == 0) {
        supported_rates.push_back(static_cast<double>(test_rates[i]));
      }
    }
  }

  snd_pcm_close(pcm);
  return supported_rates;
}

// Helper to get channel count for an ALSA device
static int getAlsaChannelCount(const char* device_name, bool is_capture) {
  snd_pcm_t* pcm = nullptr;
  snd_pcm_hw_params_t* params = nullptr;
  int channels = 0;

  int err = snd_pcm_open(&pcm, device_name,
                         is_capture ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                         SND_PCM_NONBLOCK);
  if (err < 0) {
    return 0;
  }

  snd_pcm_hw_params_alloca(&params);
  if (snd_pcm_hw_params_any(pcm, params) >= 0) {
    unsigned int max_ch = 0;
    snd_pcm_hw_params_get_channels_max(params, &max_ch);
    channels = static_cast<int>(max_ch);
  }

  snd_pcm_close(pcm);
  return channels;
}

audiodeviceinfo_list_t enumerateAudioDevices_alsa() {
  audiodeviceinfo_list_t result;

  // Enumerate ALSA PCM devices
  void **hints;
  if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
    return result;
  }

  int device_index = 0;
  for (void **hint = hints; *hint; ++hint) {
    char *name = snd_device_name_get_hint(*hint, "NAME");
    char *desc = snd_device_name_get_hint(*hint, "DESC");
    char *ioid = snd_device_name_get_hint(*hint, "IOID");

    if (name) {
      // Skip null entries
      if (strncmp(name, "null", 4) != 0) {
        // Determine if input, output, or both
        bool is_input = true;
        bool is_output = true;
        if (ioid) {
          if (strcmp(ioid, "Input") == 0) {
            is_output = false;
          } else if (strcmp(ioid, "Output") == 0) {
            is_input = false;
          }
        }

        // Get channel counts
        int input_channels = is_input ? getAlsaChannelCount(name, true) : 0;
        int output_channels = is_output ? getAlsaChannelCount(name, false) : 0;

        // Probe sample rates
        std::vector<double> input_rates;
        std::vector<double> output_rates;
        if (input_channels > 0) {
          input_rates = probeAlsaSampleRates(name, true);
        }
        if (output_channels > 0) {
          output_rates = probeAlsaSampleRates(name, false);
        }

        // Collect all unique rates
        std::set<double> all_rates;
        for (auto r : input_rates) all_rates.insert(r);
        for (auto r : output_rates) all_rates.insert(r);

        // If no rates probed, assume 48000
        if (all_rates.empty()) {
          all_rates.insert(48000.0);
        }

        // Create an entry for each supported sample rate
        for (double rate : all_rates) {
          auto info = std::make_shared<AudioDeviceInfo>();
          info->_name = name;
          info->_device_index = device_index;
          info->_sample_rate = rate;
          info->_supported_input_rates = input_rates;
          info->_supported_output_rates = output_rates;

          // Only set channels if this rate is supported for that direction
          bool rate_supported_input = std::find(input_rates.begin(), input_rates.end(), rate) != input_rates.end();
          bool rate_supported_output = std::find(output_rates.begin(), output_rates.end(), rate) != output_rates.end();

          if (rate_supported_input || input_rates.empty()) {
            info->_max_input_channels = input_channels;
          }
          if (rate_supported_output || output_rates.empty()) {
            info->_max_output_channels = output_channels;
          }

          result.push_back(info);
        }

        device_index++;
      }
      free(name);
    }
    if (desc) free(desc);
    if (ioid) free(ioid);
  }

  snd_device_name_free_hint(hints);
  return result;
}

} // namespace ork::lev2
#endif
