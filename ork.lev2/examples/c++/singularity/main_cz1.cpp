////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/file/path.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/cz1.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::audio::singularity;

namespace ork::lev2 {
//void startupAudio();
//void tearDownAudio();
} // namespace ork::lev2

bool TEST = false;

int main(int argc, char** argv) {
  auto basepath = basePath() / "casioCZ";
  //startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  auto add_event = [&](prgdata_constptr_t prog, //
                       float time,
                       float duration,
                       int midinote) {
    synth::instance()->addEvent(time, [=]() {
      // NOTE ON
      auto noteinstance = synth::instance()->keyOn(midinote, 127, prog);
      assert(noteinstance);
      // NOTE OFF
      synth::instance()->addEvent(time + duration, [=]() { //
        synth::instance()->keyOff(noteinstance);
      });
    });
  };
  //////////////////////////////////////////////////////////////////////////////
  if (TEST) {
    auto czdata = CzData::load(basepath / "edit.syx", "bank1");
    for (int i = 0; i < 2; i++) {
      auto prg = czdata->getProgram(0);
      add_event(prg, float(0) * 2, 4.0, 36 + i * 12);
    }
    usleep(35 << 20); // just wait, let the "music" play..
  } else {
    auto czdata1 = CzData::load(basepath / "factoryA.bnk", "bank1");
    auto czdata2 = CzData::load(basepath / "factoryB.bnk", "bank2");
    for (int i = 0; i < 64; i++) { // 2 32 patch banks
      auto bnk = (i >> 5) ? czdata2 : czdata1;
      auto prg = bnk->getProgram(i % 32);
      printf("i<%d> prg<%p>\n", i, (void*) prg.get());
      add_event(prg, float(i) * 0.5, 1.0, 36);
    }
    usleep(35 << 20); // just wait, let the "music" play..
  }
  //////////////////////////////////////////////////////////////////////////////
  //tearDownAudio();
  return 0;
}
