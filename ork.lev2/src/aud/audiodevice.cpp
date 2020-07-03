////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/math/audiomath.h>
#include <ork/reflect/properties/register.h>

///////////////////////////////////////////////////////////////////////////////

#include "null/audiodevice_null.h"
#include "portaudio/audiodevice_pa.h"
#define NativeDevice AudioDevicePa

bool gb_audio_filter = false;

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<int, ork::PoolString>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

AudioDevice* AudioDevice::gpDevice = 0;

AudioDevice* AudioDevice::GetDevice(void) {
  if (0 == gpDevice) {
    if (true) // gb_audio_filter )
    {
      gpDevice = new AudioDeviceNULL;
    } else {
      gpDevice = new NativeDevice;
    }
  }

  return gpDevice;
}

///////////////////////////////////////////////////////////////////////////////

AudioDevice::AudioDevice() {
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
