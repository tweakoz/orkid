#include "harness.h"
#include <ork/lev2/aud/singularity/cz1.h>

int main(int argc, char** argv) {
  auto qtapp    = createEZapp(argc, argv);
  auto basepath = basePath() / "casioCZ";
  startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  auto czdata = std::make_shared<CzData>();
  // czdata->loadBank(basepath / "edit.syx", "bank1");
  czdata->loadBank(basepath / "factoryA.bnk", "bank1");
  czdata->loadBank(basepath / "factoryB.bnk", "bank2");
  for (int i = 1; i < 64; i++) { // 2 32 patch banks
    auto prg = czdata->getProgram(i);
    for (int n = 0; n < 12; n++) {
      enqueue_audio_event(prg, float(i * 12 + n) * 0.5, 1.0, 36 + (n % 12));
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
