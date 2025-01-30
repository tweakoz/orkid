#include <ork/lev2/aud/audiodevice.h>

int main(int argc, char** argv){

  auto dev = ork::lev2::AudioDevice::instance();
  printf("AudioDevice instance created <%p>\n", dev.get());
  dev->ShutdownNow();
  return 0;
}