#include <QWindow>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/spawner.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.inl>
#include <iomanip>
#include <iostream>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

//////////////////////////////////////////////////////////
// read colorimeter
//////////////////////////////////////////////////////////

fvec3 spotreadYUV() {

  // todo : keep spotread open and stream
  //   repeated measurements
  //   to amortize colorimeter startup time costs

  fvec3 rval;
  const int MAX_BUFFER = 255;
  std::string stdout;
  char buffer[MAX_BUFFER];
  FILE* stream = popen("spotread -e -O -T -u", "r");
  while (fgets(buffer, MAX_BUFFER, stream) != NULL)
    stdout.append(buffer);
  pclose(stream);
  auto lines = SplitString(stdout, '\n');
  for (auto l : lines) {
    auto it = l.find("Yuv");
    if (it != std::string::npos) {
      auto lsub = l.substr(it);
      sscanf(lsub.c_str(), "Yuv: %f %f %f", &rval.x, &rval.y, &rval.z);
    }
  }
  return rval;
}

//////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  auto qtapp = OrkEzQtApp::create(argc, argv);
  //////////////////////////////////////////////////////////
  deco::printf(fvec3::White(), "reading colorimeter, please wait...\n");
  fvec3 yuv = spotreadYUV();
  deco::printf(fvec3::Yellow(), "y<%g cd/m^2> u<%g> v<%g>\n", yuv.x, yuv.y, yuv.z);
  //////////////////////////////////////////////////////////
  // project private asset filedevctx
  // so we can load our private shader
  //////////////////////////////////////////////////////////
  auto this_dir  = file::Path::orkroot_dir() / "ork.lev2" / "utils" / "colorcheck";
  auto ccheckctx = qtapp->newFileDevContext("colorcheck://");
  ccheckctx->setFilesystemBaseAbs(this_dir);
  ccheckctx->SetPrependFilesystemBase(true);
  printf("ccheckpath<%s>\n", this_dir.c_str());
  //////////////////////////////////////////////////////////
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  FreestyleMaterial material;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  const FxShaderParam* fxparameterT    = nullptr;
  float t                              = 0.0f;
  //////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "colorcheck://shader");
    fxtechnique    = material.technique("yuv2rgb");
    fxparameterMVP = material.param("mvp");
    fxparameterT   = material.param("t");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterT<%p>\n", fxparameterT);
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
    material.bindParamMatrix(fxparameterMVP, fmtx4::Identity);
    material.bindParamFloat(fxparameterT, fmod(t, 1));
    gfxwin->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0), fvec4(0));
    material.end(RCFD);
    context->endFrame();
    t += 0.001f;
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
