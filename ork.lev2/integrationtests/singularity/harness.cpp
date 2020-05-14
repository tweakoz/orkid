#include "harness.h"
#include <boost/program_options.hpp>
#include <iostream>
namespace po = boost::program_options;

#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
}
#endif

static auto the_synth = synth::instance();

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
    the_synth->onDrawHud(context, TARGW, TARGH);
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
    bool isalt  = ev->mbALT;
    bool isctrl = ev->mbCTRL;
    switch (ev->miEventCode) {
      case ui::UIEV_KEY:
      case ui::UIEV_KEY_REPEAT:
        switch (ev->miKeyCode) {
          case 'p':
            the_synth->_hudpage = (the_synth->_hudpage + 1) % 2;
            break;
          case '-': {
            int64_t amt         = isalt ? 100 : (isctrl ? 1 : 10);
            the_synth->_oswidth = std::clamp(the_synth->_oswidth - amt, int64_t(0), int64_t(4095));
            break;
          }
          case '=': {
            int64_t amt         = isalt ? 100 : (isctrl ? 1 : 10);
            the_synth->_oswidth = std::clamp(the_synth->_oswidth + amt, int64_t(0), int64_t(4095));
            break;
          }
          case '[': {
            float amt             = isalt ? 0.1 : (isctrl ? 0.001 : 0.01);
            the_synth->_ostriglev = std::clamp(the_synth->_ostriglev - amt, -1.0f, 1.0f);
            break;
          }
          case ']': {
            float amt             = isalt ? 0.1 : (isctrl ? 0.001 : 0.01);
            the_synth->_ostriglev = std::clamp(the_synth->_ostriglev + amt, -1.0f, 1.0f);
            break;
          }
          case '\\': {
            the_synth->_ostrigdir = !the_synth->_ostrigdir;
            break;
          }
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
