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

bool TEST = false;

int main(int argc, char** argv) {
  auto basepath = file::Path::share_dir() / "singularity" / "casioCZ";
  startupAudio();
  the_synth->resetFenables();
  //////////////////////////////////////////////////////////////////////////////
  // boot up debug HUD
  //////////////////////////////////////////////////////////////////////////////
  auto qtapp  = OrkEzQtApp::create(argc, argv);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  FreestyleMaterial material;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  const FxShaderParam* fxparameterMODC = nullptr;
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "orkshader://solid");
    fxtechnique     = material.technique("mmodcolor");
    fxparameterMVP  = material.param("MatMVP");
    fxparameterMODC = material.param("modcolor");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterMODC<%p>\n", fxparameterMODC);
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context        = drwev->GetTarget();
    auto fbi            = context->FBI(); // FrameBufferInterface
    auto fxi            = context->FXI(); // FX Interface
    int TARGW           = context->mainSurfaceWidth();
    int TARGH           = context->mainSurfaceHeight();
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);

    fbi->SetClearColor(fvec4(0.2, 0.0, 0.1, 1));
    ////////////////////////////////////////////////////
    // draw the synth HUD
    ////////////////////////////////////////////////////
    context->beginFrame();
    the_synth->onDrawHud(context, TARGW, TARGH);
    context->endFrame();
    ////////////////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) { //
    printf("GOTRESIZE<%d %d>\n", w, h);
  });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->mEventCode) {
      case ui::UIEV_PUSH:
        OrkAssert(false);
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  //////////////////////////////////////////////////////////////////////////////
  auto enqueue_audio_event = [&](const programData* prog, //
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
      enqueue_audio_event(prg, float(0) * 2, 4.0, 36 + i * 12);
    }
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
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  qtapp->exec();
  tearDownAudio();
  return 0;
}
