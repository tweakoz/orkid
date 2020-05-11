#include <QWindow>
#include <portaudio.h>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/file/path.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::audio::singularity;

namespace ork::lev2 {
void startupAudio();
void tearDownAudio();
extern synth* the_synth;
} // namespace ork::lev2


qtezapp_ptr_t createEZapp(int& argc, char** argv);

inline void enqueue_audio_event(const ProgramData* prog, //
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
}
