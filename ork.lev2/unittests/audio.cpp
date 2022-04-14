////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <utpp/UnitTest++.h>

TEST(Audio1) {
  auto paudio = ork::lev2::AudioDevice::instance();
  printf("audiodev<%p>\n", (void*)paudio.get());
  CHECK(paudio != nullptr);
}
