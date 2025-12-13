////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/tempstring.h>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/kernel/netpacket_serdes.inl>
#include <ork/kernel/environment.h>
#include <ork/math/audiomath.h>
#include <ork/reflect/properties/register.h>
#include <map>
#include <functional>
#include <algorithm>

///////////////////////////////////////////////////////////////////////////////

#include "null/audiodevice_null.h"
///////////////////////////////////////
#if defined(ENABLE_ALSA)
#include "alsa/audiodevice_alsa.h"
#endif
///////////////////////////////////////
#if defined(ENABLE_PORTAUDIO)
#include "portaudio/audiodevice_pa.h"
#endif
///////////////////////////////////////
#if defined(ENABLE_PIPEWIRE)
#include "pipewire/audiodevice_pipewire.h"
#endif
///////////////////////////////////////
#if defined(ENABLE_CORE_AUDIO)
#include "coreaudio/CoreAudioDevice.h"
#endif
///////////////////////////////////////
#include <ork/lev2/aud/stream/audiodevice_stream.h>
///////////////////////////////////////

bool gb_audio_filter = false;

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<int, ork::PoolString>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

AudioInputChunk::AudioInputChunk(size_t numch) 
 : _chunk_index(0)
 , _num_frames(0) {
 setNumChannels(numch);
}

///////////////////////////////////////////////////////////////////////////////

void AudioInputChunk::setNumChannels(size_t channel_count){
  _channels.resize(channel_count);
}

///////////////////////////////////////////////////////////////////////////////

static audiodevice_ptr_t g_audio_device = nullptr;

struct AudioDevFactory {

  AudioDevFactory(appinitdata_wkptr_t aid){

    std::string default_device_type = "PORTAUDIO";
    #if defined(ENABLE_CORE_AUDIO)
    default_device_type = "COREAUDIO";
    #endif

    std::string device_type = default_device_type;
    if( auto appinitd = aid.lock() ){
      if( appinitd->_audio_ioclass != "default" ){
        device_type = appinitd->_audio_ioclass;
      }
    }

    #if defined(ENABLE_ALSA)
    if( device_type == "ALSA" ){
      _device = std::make_shared<AudioDeviceAlsa>(aid);
    }
#endif
#if defined(ENABLE_CORE_AUDIO)
    if( device_type == "COREAUDIO" ){
      _device = std::make_shared<ca::CoreAudioDevice>(aid);
    }
#endif
#if defined(ENABLE_PORTAUDIO)
    if( device_type == "PORTAUDIO" ){
      _device = std::make_shared<AudioDevicePa>(aid);
    }
#endif
#if defined(ENABLE_PIPEWIRE)
    if( device_type == "PIPEWIRE" ){
      _device = std::make_shared<pipewire::AudioDevicePipeWire>(aid);
    }
#endif
    if( device_type == "STREAM" ){
      _device = std::make_shared<StrAudioDevice>(aid);
    }

    if(nullptr == _device ){
      _device = std::make_shared<AudioDeviceNULL>(aid);
    }
    //printf("AudioDevFactory: audio ioclass<%s>\n",device_type.c_str());

    g_audio_device = _device;
  }

  audiodevice_ptr_t _device;
};

using audiodevfactory_ptr_t = std::shared_ptr<AudioDevFactory>;

audiodevice_ptr_t AudioDevice::createInstance(appinitdata_wkptr_t aid) {
  AudioDevFactory devf(aid);
  return devf._device;
}

///////////////////////////////////////////////////////////////////////////////

audiodevice_ptr_t AudioDevice::getInstance() {
  return g_audio_device;
}

///////////////////////////////////////////////////////////////////////////////

AudioDevice::AudioDevice(appinitdata_wkptr_t appinitd)
  : _appinitdata(appinitd) {
  _vars = std::make_shared<varmap::VarMap>();
}

AudioDevice::~AudioDevice() {
}

void AudioDevice::startup() {}
void AudioDevice::shutdown() {}

///////////////////////////////////////////////////////////////////////////////

void StreamingAudioInputChunkSource::start() {

}
void StreamingAudioInputChunkSource::stop() {

}
lev2::audioinputchunk_ptr_t StreamingAudioInputChunkSource::getChunk() {
  lev2::audioinputchunk_ptr_t chunk = nullptr;
  _inputqueue.try_pop(chunk);
  return chunk;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Platform-specific enumeration functions are defined in their respective files
// (coreaudio/, portaudio/, pipewire/, alsa/)
///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_CORE_AUDIO)
audiodeviceinfo_list_t enumerateAudioDevices_coreaudio();
#endif
#if defined(ENABLE_PORTAUDIO)
audiodeviceinfo_list_t enumerateAudioDevices_portaudio();
#endif
#if defined(ENABLE_PIPEWIRE)
audiodeviceinfo_list_t enumerateAudioDevices_pipewire();
#endif
#if defined(ENABLE_ALSA)
audiodeviceinfo_list_t enumerateAudioDevices_alsa();
#endif

audiodeviceinfo_list_t enumerateAudioDevices() {
  audiodeviceinfo_list_t result;
#if defined(ENABLE_CORE_AUDIO)
  result = enumerateAudioDevices_coreaudio();
#elif defined(ENABLE_PORTAUDIO)
  result = enumerateAudioDevices_portaudio();
#elif defined(ENABLE_PIPEWIRE)
  result = enumerateAudioDevices_pipewire();
#elif defined(ENABLE_ALSA)
  result = enumerateAudioDevices_alsa();
#endif

  // Generate stable 4-char base36 hash from direction, name, sample rate, and channels
  auto makeHash4 = [](const std::string& direction, const std::string& name, double sr, int channels) -> std::string {
    std::string input = direction + ":" + name + ":" + std::to_string(int(sr)) + ":" + std::to_string(channels);
    size_t h = std::hash<std::string>{}(input);

    static const char base36[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string hash;
    for (int i = 0; i < 4; i++) {
      hash += base36[h % 36];
      h /= 36;
    }
    return hash;
  };

  // Filter: inputs must have 1 or 2 channels, outputs must have 2 channels, SR must be 48000
  audiodeviceinfo_list_t filtered;
  for (auto& dev : result) {
    bool valid_input = (dev->_max_input_channels == 1 || dev->_max_input_channels == 2) &&
                       (int(dev->_sample_rate) == 48000);
    bool valid_output = (dev->_max_output_channels == 2) &&
                        (int(dev->_sample_rate) == 48000);

    if (valid_input || valid_output) {
      // Only keep channels that match the filter
      if (!valid_input) {
        dev->_max_input_channels = 0;
      }
      if (!valid_output) {
        dev->_max_output_channels = 0;
      }
      filtered.push_back(dev);
    }
  }
  result = filtered;

  // Assign short IDs based on stable hash (4-char only, no prefix)
  for (auto& dev : result) {
    if (dev->_max_input_channels > 0) {
      dev->_input_short_id = makeHash4("I", dev->_name, dev->_sample_rate, dev->_max_input_channels);
    }
    if (dev->_max_output_channels > 0) {
      dev->_output_short_id = makeHash4("O", dev->_name, dev->_sample_rate, dev->_max_output_channels);
    }
  }

  // Sort by short ID for stable ordering
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    std::string id_a = a->_input_short_id.empty() ? a->_output_short_id : a->_input_short_id;
    std::string id_b = b->_input_short_id.empty() ? b->_output_short_id : b->_input_short_id;
    return id_a < id_b;
  });

  return result;
}

///////////////////////////////////////////////////////////////////////////////

audiodeviceinfo_ptr_t findAudioDeviceByShortId(const std::string& short_id) {
  auto devices = enumerateAudioDevices();
  for (const auto& dev : devices) {
    if (dev->_input_short_id == short_id || dev->_output_short_id == short_id) {
      return dev;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
