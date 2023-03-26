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
  auto paudio = ork::lev2::AudioDevice::instance();
  printf("audiodev<%p>\n", (void*)paudio.get());
  CHECK(paudio != nullptr);
}
