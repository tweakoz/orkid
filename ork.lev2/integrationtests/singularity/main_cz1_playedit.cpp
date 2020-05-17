#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>

int main(int argc, char** argv) {
  auto qtapp    = createEZapp(argc, argv);
  auto basepath = basePath() / "casioCZ";
  startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  auto bnk = CzData::load(basepath / "edit.syx", "bank1");
  auto prg = bnk->getProgram(0);
  for (int i = 24; i < 72; i++) {
    enqueue_audio_event(prg, 1 + float(i - 24) * 1.0, 1.0, i);
  }
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
