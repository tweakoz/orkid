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
///////////////////////////////////////
#endif

bool gb_audio_filter = false;

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<int, ork::PoolString>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudioDevFactory{

  AudioDevFactory(){

    std::string default_device_type = "PORTAUDIO";

    auto stagedir = ork::file::Path::stage_dir();
    auto orkconfig_path = stagedir / "orkid.json";
    printf( "orkconfig_path<%s>\n", orkconfig_path.c_str());
    if( orkconfig_path.doesPathExist() ){
      ork::net::serdes::val_t out_val;
      ork::net::serdes::valueFromJsonFile(out_val,orkconfig_path);
      auto& orkconfig = out_val.get<ork::net::serdes::kvmap_t>();
      auto it = orkconfig.find("AUDIODEVICE");
      if( it != orkconfig.end() ){
        default_device_type = it->second.get<std::string>();
      }
    }

#if defined(ENABLE_ALSA)
    if( default_device_type == "ALSA" ){
      _device = std::make_shared<AudioDeviceAlsa>();
    }
#endif
#if defined(ENABLE_PORTAUDIO)
    if( default_device_type == "PORTAUDIO" ){
      _device = std::make_shared<AudioDevicePa>();
    }
#endif
#if defined(ENABLE_PIPEWIRE)
    if( default_device_type == "PIPEWIRE" ){
      _device = std::make_shared<pipewire::AudioDevicePipeWire>();
    }
#endif
    
    if(nullptr == _device ){
      _device = std::make_shared<AudioDeviceNULL>();
    }
  }

  audiodevice_ptr_t _device;
};

using audiodevfactory_ptr_t = std::shared_ptr<AudioDevFactory>;

audiodevice_ptr_t AudioDevice::instance(void) {
  static auto devfactory = std::make_shared<AudioDevFactory>();
  return devfactory->_device;
}

///////////////////////////////////////////////////////////////////////////////

AudioDevice::AudioDevice() {
}

AudioDevice::~AudioDevice() {
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
