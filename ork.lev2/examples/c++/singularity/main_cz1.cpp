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

int main(int argc, char** argv) {
  auto basepath = file::Path::share_dir() / "singularity" / "casioCZ";
  startupAudio();
  auto czdata = std::make_shared<CzData>(the_synth);
  czdata->loadBank(basepath / "cz1_1.syx", "c1");
  auto prg1 = czdata->getProgram(0);
  the_synth->resetFenables();
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
  add_event(prg1, 1.0, 30.0, 48);
  //////////////////////////////////////////////////////////////////////////////
  usleep(60 << 20); // just wait, let the "music" play..
  //////////////////////////////////////////////////////////////////////////////
  tearDownAudio();
  return 0;
}
