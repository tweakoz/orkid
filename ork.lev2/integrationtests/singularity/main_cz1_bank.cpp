#include "harness.h"

int main(int argc, char** argv) {
  auto qtapp    = createEZapp(argc, argv);
  auto basepath = file::Path::share_dir() / "singularity" / "casioCZ";
  startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  auto czdata = std::make_shared<CzData>(the_synth);
  bool TEST   = true;
  if (TEST) {
    // czdata->loadBank(basepath / "edit.syx", "bank1");
    czdata->loadBank(basepath / "factoryA.bnk", "bank1");
    auto prg = czdata->getProgram(0);
    enqueue_audio_event(prg, float(0) * 2, 60.0, 48);
  } else {
    czdata->loadBank(basepath / "factoryA.bnk", "bank1");
    czdata->loadBank(basepath / "factoryB.bnk", "bank2");
    for (int i = 0; i < 64; i++) { // 2 32 patch banks
      auto prg = czdata->getProgram(i);
      printf("i<%d> prg<%p>\n", i, prg);
      enqueue_audio_event(prg, float(i) * 0.5, 1.0, 44 - 12);
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, 0});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
