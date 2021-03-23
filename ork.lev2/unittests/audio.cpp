#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <utpp/UnitTest++.h>

TEST(Audio1) {
  auto paudio = ork::lev2::AudioDevice::instance();
  printf("audiodev<%p>\n", paudio.get());
  CHECK(paudio != nullptr);
}
