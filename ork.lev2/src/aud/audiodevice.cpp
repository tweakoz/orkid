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

audiodeviceinfo_list_t enumerateAudioDevices() {
  audiodeviceinfo_list_t result;

#if defined(ENABLE_CORE_AUDIO)
  // CoreAudio enumeration
  ca::AudioDeviceList input_list(true);
  ca::AudioDeviceList output_list(false);

  std::map<std::string, audiodeviceinfo_ptr_t> device_map;

  for (const auto& item : input_list.GetMap()) {
    auto info = std::make_shared<AudioDeviceInfo>();
    info->_name = item.first;
    info->_max_input_channels = item.second->countChannels();
    info->_default_sample_rate = item.second->_format.mSampleRate;
    device_map[item.first] = info;
  }

  for (const auto& item : output_list.GetMap()) {
    auto it = device_map.find(item.first);
    if (it != device_map.end()) {
      it->second->_max_output_channels = item.second->countChannels();
    } else {
      auto info = std::make_shared<AudioDeviceInfo>();
      info->_name = item.first;
      info->_max_output_channels = item.second->countChannels();
      info->_default_sample_rate = item.second->_format.mSampleRate;
      device_map[item.first] = info;
    }
  }

  for (const auto& item : device_map) {
    result.push_back(item.second);
  }

#elif defined(ENABLE_PORTAUDIO)
  // PortAudio enumeration
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

#elif defined(ENABLE_PIPEWIRE)
  // PipeWire - basic enumeration not easily available without running loop
  // Return empty list for now - PipeWire uses node names dynamically

#elif defined(ENABLE_ALSA)
  // ALSA enumeration would go here

#endif

  return result;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
