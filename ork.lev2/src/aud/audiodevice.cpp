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
#include <ork/kernel/environment.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/netpacket_serdes.inl>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/math/audiomath.h>
#include <ork/reflect/properties/register.h>

///////////////////////////////////////////////////////////////////////////////

#include "null/audiodevice_null.h"
#if defined(ENABLE_ALSA)
#include "alsa/audiodevice_alsa.h"
#elif defined(ENABLE_PORTAUDIO)
#include "portaudio/audiodevice_pa.h"
#endif

bool gb_audio_filter = false;

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<int, ork::PoolString>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudioDeviceGenerator{

  AudioDeviceGenerator(){

    OrkAssert(false);

    //////////////////////////////
    // default backend
    //////////////////////////////

    using device_factory_t = std::function<audiodevice_ptr_t()>;
    using devicefactory_map_t = std::unordered_map<std::string, device_factory_t>;
    static devicefactory_map_t _device_factories;

    #if defined(__APPLE__)
    static std::string _audio_backend = "PORTAUDIO";
    #else
    static std::string _audio_backend = "ALSA";
    #endif

    //////////////////////////////
    // fetch config file if it exists
    //////////////////////////////

    std::string stagedir;
    ork::genviron.get("OBT_STAGE", stagedir);
    auto orkconfig_path = ork::file::Path(stagedir) / "orkconfig.json";
    printf( "orkconfig_path<%s>\n", orkconfig_path.c_str());
    if( orkconfig_path.doesPathExist() ){
      ork::net::serdes::val_t out_val;
      ork::net::serdes::valueFromJsonFile(out_val,orkconfig_path);
      auto& orkconfig = out_val.get<ork::net::serdes::kvmap_t>();
      auto it = orkconfig.find("AUDIO_BACKEND");
      if( it != orkconfig.end() ){
        _audio_backend = it->second.get<std::string>();
        printf( "OVERRIDE AUDIO_BACKEND<%s>\n", _audio_backend.c_str() );
      }
    }

    //////////////////////////////
    // register factories
    //////////////////////////////

    #if defined(ENABLE_ALSA)
      _device_factories["ALSA"] = []() -> audiodevice_ptr_t { 
          return std::make_shared<AudioDeviceAlsa>();
      };
    #endif
    #if defined(ENABLE_PORTAUDIO)
      _device_factories["PORTAUDIO"] = []() -> audiodevice_ptr_t { 
          return std::make_shared<AudioDevicePa>();
      };
    #endif

    //////////////////////////////
    // find factory
    //////////////////////////////

    auto it = _device_factories.find(_audio_backend);
    OrkAssert(it != _device_factories.end());
    _device = it->second();

  }

  audiodevice_ptr_t _device;

};

///////////////////////////////////////////////////////////////////////////////

audiodevice_ptr_t AudioDevice::instance() {
  static auto generator = std::make_shared<AudioDeviceGenerator>();
  return generator->_device;
}

///////////////////////////////////////////////////////////////////////////////

AudioDevice::AudioDevice() {
}

AudioDevice::~AudioDevice() {
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
