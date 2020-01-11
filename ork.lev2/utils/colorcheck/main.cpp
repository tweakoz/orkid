#include <QWindow>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.inl>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
int main(int argc, char** argv) {
  auto qtapp  = OrkEzQtApp::create(argc, argv);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  FreestyleMaterial material;
  FxShader* fxshader                   = nullptr;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  const FxShaderParam* fxparameterMODC = nullptr;
  float t                              = 0.0f;
  //////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "orkshader://solid");
    fxshader        = material._shader;
    fxtechnique     = material.technique("mmodcolor");
    fxparameterMVP  = material.param("MatMVP");
    fxparameterMODC = material.param("modcolor");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterMODC<%p>\n", fxparameterMODC);
    qtwin->_ctqt->pushRefreshPolicy(RefreshPolicyItem{EREFRESH_FIXEDFPS, 60});
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context        = drwev.GetTarget();
    auto fbi            = context->FBI(); // FrameBufferInterface
    auto fxi            = context->FXI(); // FX Interface
    int TARGW           = context->mainSurfaceWidth();
    int TARGH           = context->mainSurfaceHeight();
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);

    fbi->SetClearColor(fvec4::Black());
    context->beginFrame();
    RenderContextFrameData RCFD(context);
    material.bindTechnique(fxtechnique);
    material.begin(RCFD);
    fxi->BindParamMatrix(fxshader, fxparameterMVP, fmtx4::Identity);
    // todo : compute in shader..
    for (int u = 0; u < 256; u++) {
      for (int v = 0; v < 256; v++) {
        float fu  = float(u) / 256.0f;
        float fv  = float(v) / 256.0f;
        float fx1 = -1.0f + 2.0 * fu;
        float fy1 = -1.0f + 2.0 * fv;
        float fw  = 2.0 / 256.0f;
        float fh  = 2.0 / 256.0f;
        fvec3 rgb;
        rgb.setYUV(fmod(t, 1.0), fu, fv);
        fxi->BindParamVect4(fxshader, fxparameterMODC, rgb);
        gfxwin->Render2dQuadEML(fvec4(fx1, fy1, fw, fh), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
      }
    }
    material.end(RCFD);
    context->endFrame();
    t += 0.03f;
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) { printf("GOTRESIZE<%d %d>\n", w, h); });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([&](const ui::Event& ev) -> ui::HandlerResult {
    switch (ev.mEventCode) {
      case ui::UIEV_DOUBLECLICK:
        OrkAssert(false);
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  //////////////////////////////////////////////////////////
  return qtapp->exec();
}
