#include <QWindow>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/file/path.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <portaudio.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::audio::singularity;

namespace ork::lev2 {
void startupAudio();
void tearDownAudio();
extern synth* the_synth;
} // namespace ork::lev2

bool TEST = false;

int main(int argc, char** argv) {
  auto basepath = file::Path::share_dir() / "singularity" / "casioCZ";
  startupAudio();
  //////////////////////////////////////////////////////////////////////////////
  auto add_event = [&](const programData* prog, //
                       float time,
                       float duration,
                       int midinote) {
    the_synth->addEvent(time, [=]() {
      // NOTE ON
      auto noteinstance = the_synth->keyOn(midinote, prog);
      assert(noteinstance);
      // NOTE OFF
      the_synth->addEvent(time + duration, [=]() { //
        the_synth->keyOff(noteinstance);
      });
    });
  };
  //////////////////////////////////////////////////////////////////////////////
  auto czdata = std::make_shared<CzData>(the_synth);
  if (TEST) {
    czdata->loadBank(basepath / "edit.syx", "bank1");
    for (int i = 0; i < 2; i++) {
      auto prg = czdata->getProgram(0);
      add_event(prg, float(0) * 2, 4.0, 36 + i * 12);
    }
    usleep(35 << 20); // just wait, let the "music" play..
  } else {
    czdata->loadBank(basepath / "factoryA.bnk", "bank1");
    czdata->loadBank(basepath / "factoryB.bnk", "bank2");
    for (int i = 0; i < 64; i++) { // 2 32 patch banks
      auto prg = czdata->getProgram(i);
      printf("i<%d> prg<%p>\n", i, prg);
      add_event(prg, float(i) * 0.5, 1.0, 36);
    }
    usleep(35 << 20); // just wait, let the "music" play..
  }
  //////////////////////////////////////////////////////////////////////////////
  tearDownAudio();
  return 0;
}
