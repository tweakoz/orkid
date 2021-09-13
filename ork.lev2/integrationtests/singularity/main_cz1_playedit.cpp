#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>

int main(int argc, char** argv) {
  auto app      = createEZapp(argc, argv);
  auto basepath = basePath() / "casioCZ";
  //////////////////////////////////////////////////////////////////////////////
  auto bnk = CzData::load(basepath / "edit.syx", "bank1");
  auto prg = bnk->getProgram(0);

  for (int i = 0; i < 128; i++) {
    int note = 36 + i % 24;
    enqueue_audio_event(prg, 1 + float(i) * 1.0, 1.0, note);
  }
  //////////////////////////////////////////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FASTEST, 0});
  app->runloop();
  return 0;
}
