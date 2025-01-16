////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/orkpool.h>
#include <ork/lev2/aud/audiodevice.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct AudioDevicePa final : public AudioDevice {
public:
  AudioDevicePa(appinitdata_wkptr_t appinitd);
  ~AudioDevicePa() final;
  void startup() final;
  void shutdown() final;

  ::ork::audio::singularity::synth_ptr_t _the_synth;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
