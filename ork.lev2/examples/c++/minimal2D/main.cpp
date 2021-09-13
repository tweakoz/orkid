#include <ork/kernel/string/deco.inl>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
int main(int argc, char** argv) {
  auto qtapp  = OrkEzQtApp::create(argc, argv);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  FreestyleMaterial material;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  const FxShaderParam* fxparameterMODC = nullptr;
  //////////////////////////////////////////////////////////
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
    float r             = float(rand() % 256) / 255.0f;
    float g             = float(rand() % 256) / 255.0f;
    float b             = float(rand() % 256) / 255.0f;
    int TARGW           = context->mainSurfaceWidth();
    int TARGH           = context->mainSurfaceHeight();
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);

    fbi->SetClearColor(fvec4(r, g, b, 1));
    context->beginFrame();
    RenderContextFrameData RCFD(context);
    material.begin(fxtechnique, RCFD);
    material.bindParamMatrix(fxparameterMVP, fmtx4::Identity());
    material.bindParamVec4(fxparameterMODC, fvec4::Red());
    gfxwin->Render2dQuadEML(fvec4(-0.5, -0.5, 1, 1), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
    material.end(RCFD);
    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) { printf("GOTRESIZE<%d %d>\n", w, h); });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev->_eventcode) {
      case ui::EventCode::DOUBLECLICK:
        OrkAssert(false);
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  //////////////////////////////////////////////////////////
  return qtapp->runloop();
}
