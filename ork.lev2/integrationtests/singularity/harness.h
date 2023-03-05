////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <portaudio.h>
#include <ork/application/application.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/file/path.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/midi/context.h>
#include <ork/lev2/ui/context.h>
#include <ork/kernel/timer.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::audio::singularity;

namespace ork::lev2 {
//void startupAudio();
//void tearDownAudio();
} // namespace ork::lev2

void mymidicallback(double deltatime, std::vector<unsigned char>* message, void* userData);

struct SingularityTestApp final : public OrkEzApp {
  SingularityTestApp(appinitdata_ptr_t initdata);
  ~SingularityTestApp() override;
  hudvp_ptr_t _hudvp;
};
using singularitytestapp_ptr_t = std::shared_ptr<SingularityTestApp>;

singularitytestapp_ptr_t createEZapp(appinitdata_ptr_t initdata);

inline void enqueue_audio_event(
    prgdata_constptr_t prog, //
    float time,
    float duration,
    int midinote,
    int velocity = 128) {

  auto s = synth::instance();

  if (time < s->_timeaccum) {
    time = s->_timeaccum;
  }
  //printf("time<%g> note<%d> program<%s>\n", time, midinote, prog->_name.c_str());

  s->addEvent(time, [=]() {
    // NOTE ON
    auto noteinstance = s->keyOn(midinote, velocity, prog);
    assert(noteinstance);
    // NOTE OFF
    s->addEvent(time + duration, [=]() { //
      s->keyOff(noteinstance);
    });
  });
}

prgdata_constptr_t testpattern(syndata_ptr_t syndat, int argc, char** argv);

struct Event {
  float _time = 0.0f;
  float _dur  = 0.0f;
  int _note   = 0;
  int _vel    = 0;
};
struct Sequence {
  Sequence(float tempo);
  std::vector<Event> _events;
  float _tempo = 0.0f;
  float mbs2time(int meas, int sixteenth, int clocks) const;
  void addNote(
      int meas, //
      int sixteenth,
      int clocks,
      int note,
      int vel,
      int dur = 1);
  void enqueue(prgdata_constptr_t program);
};
void seq1(float tempo, int basebar, prgdata_constptr_t program);
////////////////////////////////////////////////////////////////////////////////
struct SingularityBenchMarkApp final : public OrkEzApp {
  static constexpr size_t KNUMFRAMES = 512;
  SingularityBenchMarkApp(appinitdata_ptr_t initdata=nullptr)
      : OrkEzApp(initdata) {
  }
  ~SingularityBenchMarkApp() override {
  }
  std::vector<int> _time_histogram;
  ork::lev2::freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _fxtechniqueMODC = nullptr;
  const FxShaderTechnique* _fxtechniqueVTXC = nullptr;
  const FxShaderParam* _fxparameterMVP      = nullptr;
  const FxShaderParam* _fxparameterMODC     = nullptr;
  ork::Timer _timer;
  float _inpbuf[KNUMFRAMES * 2];
  int _numiters     = 0;
  double _cur_time  = 0.0;
  double _prev_time = 0.0;
  lev2::Font* _font;
  int _charw             = 0;
  int _charh             = 0;
  double _underrunrate   = 0;
  int _numunderruns      = 0;
  double _maxvoices      = 32.0;
  double _accumnumvoices = 0.0;
};
using singularitybenchapp_ptr_t = std::shared_ptr<SingularityBenchMarkApp>;

singularitybenchapp_ptr_t createBenchmarkApp(appinitdata_ptr_t initdata, prgdata_constptr_t program);
