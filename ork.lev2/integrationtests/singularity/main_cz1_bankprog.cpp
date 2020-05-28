#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>

int main(int argc, char** argv) {
  auto qtapp    = createEZapp(argc, argv);
  auto basepath = basePath() / "casioCZ";
  startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  auto bnk = CzData::load(basepath / "factoryA.bnk", "bank1");
  auto prg = bnk->getProgramByName("ELEC.GUITAR");
  OrkAssert(prg != nullptr);
  for (int i = 0; i < 128; i++) {
    int note = 36 + i % 24;
    enqueue_audio_event(prg, 1 + float(i) * 1.0, 1.5, note);
  }
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
