#include "harness.h"
#include <boost/program_options.hpp>
#include <iostream>
namespace po = boost::program_options;

#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
}
#endif

qtezapp_ptr_t createEZapp(int& argc, char** argv) {

  po::options_description desc("Allowed options");
  desc.add_options()                   //
      ("help", "produce help message") //
      ("hidpi", "hidpi mode");

  po::variables_map vars;
  po::store(po::parse_command_line(argc, argv, desc), vars);
  po::notify(vars);

  if (vars.count("help")) {
    std::cout << desc << "\n";
    exit(0);
  }
  if (vars.count("hidpi")) {
#if defined(__APPLE__)
    ork::lev2::_macosUseHIDPI = true;
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  // boot up debug HUD
  //////////////////////////////////////////////////////////////////////////////
  auto qtapp    = OrkEzQtApp::create(argc, argv);
  auto qtwin    = qtapp->_mainWindow;
  auto gfxwin   = qtwin->_gfxwin;
  auto material = std::make_shared<FreestyleMaterial>();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([=](Context* ctx) {
    const FxShaderTechnique* fxtechnique = nullptr;
    const FxShaderParam* fxparameterMVP  = nullptr;
    const FxShaderParam* fxparameterMODC = nullptr;
    material->gpuInit(ctx, "orkshader://solid");
    fxtechnique     = material->technique("mmodcolor");
    fxparameterMVP  = material->param("MatMVP");
    fxparameterMODC = material->param("modcolor");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterMODC<%p>\n", fxparameterMODC);
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([=](ui::drawevent_constptr_t drwev) {
    auto context        = drwev->GetTarget();
    auto fbi            = context->FBI(); // FrameBufferInterface
    auto fxi            = context->FXI(); // FX Interface
    int TARGW           = context->mainSurfaceWidth();
    int TARGH           = context->mainSurfaceHeight();
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);

    fbi->SetClearColor(fvec4(0.0, 0.0, 0.1, 1));
    ////////////////////////////////////////////////////
    // draw the synth HUD
    ////////////////////////////////////////////////////
    context->beginFrame();
    synth::instance()->onDrawHud(context, TARGW, TARGH);
    context->endFrame();
    ////////////////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([=](int w, int h) { //
    printf("GOTRESIZE<%d %d>\n", w, h);
  });
  //////////////////////////////////////////////////////////
  const int64_t trackMAX = (4095 << 16);
  qtapp->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool isshift = ev->mbALT;
    switch (ev->miEventCode) {
      case ui::UIEV_KEY:
      case ui::UIEV_KEY_REPEAT:
        switch (ev->miKeyCode) {
          case 'p':
            synth::instance()->_hudpage = (synth::instance()->_hudpage + 1) % 2;
            break;
          case '-':
            synth::instance()->_ostrack--;
            if (synth::instance()->_ostrack < -trackMAX)
              synth::instance()->_ostrack = -trackMAX;
            printf("oscope track<%d>\n", synth::instance()->_ostrack);
            break;
          case '=':
            synth::instance()->_ostrack++;
            if (synth::instance()->_ostrack > trackMAX)
              synth::instance()->_ostrack = trackMAX;
            printf("oscope track<%d>\n", synth::instance()->_ostrack);
            break;
          case '[':
            synth::instance()->_ostrack -= isshift ? 10000 : 1000;
            if (synth::instance()->_ostrack < -trackMAX)
              synth::instance()->_ostrack = -trackMAX;
            printf("oscope track<%d>\n", synth::instance()->_ostrack);
            break;
          case ']':
            synth::instance()->_ostrack += isshift ? 10000 : 1000;
            if (synth::instance()->_ostrack > trackMAX)
              synth::instance()->_ostrack = trackMAX;
            printf("oscope track<%d>\n", synth::instance()->_ostrack);
            break;
          case ';':
            synth::instance()->_oswidth--;
            if (synth::instance()->_oswidth < 0)
              synth::instance()->_oswidth = 0;
            printf("oscope width<%d>\n", synth::instance()->_oswidth);
            break;
          case '\'':
            synth::instance()->_oswidth++;
            if (synth::instance()->_oswidth > 4095)
              synth::instance()->_oswidth = 4095;
            printf("oscope width<%d>\n", synth::instance()->_oswidth);
            break;
          case ',':
            synth::instance()->_oswidth -= 100;
            if (synth::instance()->_oswidth < 0)
              synth::instance()->_oswidth = 0;
            printf("oscope width<%d>\n", synth::instance()->_oswidth);
            break;
          case '.':
            synth::instance()->_oswidth += 100;
            if (synth::instance()->_oswidth > 4095)
              synth::instance()->_oswidth = 4095;
            printf("oscope width<%d>\n", synth::instance()->_oswidth);
            break;
          default:
            break;
        }
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  return qtapp;
}
//////////////////////////////////////////////////////////////////////////////
