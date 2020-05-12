#include "harness.h"

qtezapp_ptr_t createEZapp(int& argc, char** argv) {
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
  qtapp->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->miEventCode) {
      case ui::UIEV_KEY:
        switch (ev->miKeyCode) {
          case 'p':
            the_synth->_hudpage = (the_synth->_hudpage + 1) % 2;
            break;
          case '-':
            the_synth->_ostrack--;
            printf("oscope track<%d>\n", the_synth->_ostrack);
            break;
          case '=':
            the_synth->_ostrack++;
            printf("oscope track<%d>\n", the_synth->_ostrack);
            break;
          case '[':
            the_synth->_ostrack -= 10;
            printf("oscope track<%d>\n", the_synth->_ostrack);
            break;
          case ']':
            the_synth->_ostrack += 10;
            printf("oscope track<%d>\n", the_synth->_ostrack);
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
