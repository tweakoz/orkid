////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <utpp/UnitTest++.h>

TEST(Audio1) {
  extern ork::appinitdata_ptr_t ginitdata;
  auto auddev = ork::lev2::AudioDevice::createInstance(ginitdata);
  printf("audiodev<%p>\n", (void*)auddev.get());
  CHECK(auddev != nullptr);
}
