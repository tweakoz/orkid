#include <QWindow>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
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
  // auto qtapp = OrkEzQtApp::create(argc, argv);
  startupAudio();
  auto bank     = std::make_shared<KrzSynthData>(the_synth);
  auto drums    = bank->getProgramByName("Castle_Drums");
  auto doomsday = bank->getProgramByName("Doomsday");
  auto ceetuar  = bank->getProgramByName("Cee_Tuar");
  auto piano    = bank->getProgramByName("Grand_Piano");
  auto winds    = bank->getProgramByName("Northern_Winds");
  auto sweep    = bank->getProgramByName("Hi_Res_Sweeper");
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
  // add_event(winds, 0.0, 50.0, 48);
  // add_event(sweep, 0.0, 30.0, 36);
  // add_event(sweep, 1.0, 30.0, 48);
  // add_event(sweep, 2.0, 30.0, 60);
  // add_event(doomsday, 1.0, 20.0, 36);
  // add_event(piano, 10.0, 10.0, 36);
  // add_event(piano, 10.2, 10.0, 48);
  // add_event(doomsday, 20.0, 20.0, 48);
  // add_event(doomsday, 30.0, 20.0, 60);
  add_event(ceetuar, 1.0, 5.0, 36);
  add_event(ceetuar, 2.0, 5.0, 43);
  add_event(ceetuar, 3.0, 5.0, 36);
  add_event(ceetuar, 4.0, 5.0, 48);
  for (int i = 0; i < 60; i++) {
    float t = float(i) * 1.0;
    // add_event(drums, t, 1.0, 48);
  }
  //////////////////////////////////////////////////////////////////////////////
  usleep(60 << 20); // just wait, let the "music" play..
  //////////////////////////////////////////////////////////////////////////////
  tearDownAudio();
  return 0;
}
