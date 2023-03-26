////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/orkpool.h>
#include <ork/kernel/thread.h>
#include <ork/lev2/aud/audiodevice.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

class AudioDeviceAlsa : public AudioDevice {
public:
  AudioDeviceAlsa();

protected:
  ork::svar64_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
