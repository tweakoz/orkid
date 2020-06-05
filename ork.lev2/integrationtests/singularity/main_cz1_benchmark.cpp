#include <map>
#include "harness.h"
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>

using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto the_synth = synth::instance();
  double SR      = getSampleRate();
  the_synth->setSampleRate(SR);
  the_synth->_masterGain = 0.5f;
  auto basepath          = basePath() / "casioCZ";
  //////////////////////////////////////////////////////////////////////////////
  // allocate program/layer data
  //////////////////////////////////////////////////////////////////////////////
  auto bank    = CzData::load(basepath / "factoryA.bnk", "bank1");
  auto program = bank->getProgramByName("ELEC.GUITAR");
  //////////////////////////////////////
  // benchmark the dsp program
  //////////////////////////////////////
  auto app = createBenchmarkApp(argc, argv, program);
  return app->exec();
}
