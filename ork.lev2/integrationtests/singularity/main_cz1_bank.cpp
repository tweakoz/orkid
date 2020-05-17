#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>

int main(int argc, char** argv) {
  auto qtapp    = createEZapp(argc, argv);
  auto basepath = basePath() / "casioCZ";
  startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  auto czdata1 = CzData::load(basepath / "factoryA.bnk", "bank1");
  auto czdata2 = CzData::load(basepath / "factoryB.bnk", "bank2");
  // czdata->loadBank(basepath / "edit.syx", "bank1");
  for (int i = 1; i < 64; i++) { // 2 32 patch banks
    auto bnk = (i >> 5) ? czdata2 : czdata1;
    auto prg = bnk->getProgram(i % 32);
    for (int n = 0; n < 12; n++) {
      enqueue_audio_event(prg, 3 + float(i * 12 + n) * 0.5, 1.0, 36 + (n % 12));
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
