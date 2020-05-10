#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <utpp/UnitTest++.h>

TEST(Audio1) {
  ork::lev2::AudioDevice* paudio = ork::lev2::AudioDevice::GetDevice();
  printf("audiodev<%p>\n", paudio);
  CHECK(paudio != nullptr);
}
