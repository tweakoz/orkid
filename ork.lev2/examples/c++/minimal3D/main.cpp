#include <QWindow>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/lev2/gfx/primitives.inl>

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
  Timer timer;
  primitives::Cube cube;
  cube._size        = 1;
  cube._colorTop    = fvec3(.5, 1, .5);
  cube._colorBottom = fvec3(.5, 0, .5);
  cube._colorFront  = fvec3(.5, .5, 1);
  cube._colorBack   = fvec3(.5, .5, 0);
  cube._colorLeft   = fvec3(0, .5, .5);
  cube._colorRight  = fvec3(1, .5, .5);

  //////////////////////////////////////////////////////////
  timer.Start();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "orkshader://solid");
    fxtechnique    = material.technique("vtxcolor");
    fxparameterMVP = material.param("MatMVP");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    material._rasterstate.SetCullTest(ECULLTEST_PASS_FRONT);
    cube.gpuInit(ctx);
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context = drwev.GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    auto fbi  = context->FBI();           // FrameBufferInterface
    auto fxi  = context->FXI();           // FX Interface
    auto mtxi = context->MTXI();          // matrix Interface
    auto gbi  = context->GBI();           // GeometryBuffer Interface
    ///////////////////////////////////////
    // compute view and projection matrices
    ///////////////////////////////////////
    float TARGW  = context->mainSurfaceWidth();
    float TARGH  = context->mainSurfaceHeight();
    float aspect = TARGW / TARGH;
    float phase  = timer.SecsSinceStart() * PI2 * 0.1f;
    fvec3 eye(sinf(phase) * 5.0f, 5.0f, -cosf(phase) * 5.0f);
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    float N         = -1.0f;
    float P         = +1.0f;
    auto projection = mtxi->Persp(45, aspect, 0.1, 100.0);
    auto view       = mtxi->LookAt(eye, tgt, up);
    ///////////////////////////////////////
    // Draw!
    ///////////////////////////////////////
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    context->beginFrame();

    material.bindTechnique(fxtechnique);
    material.begin(RCFD);
    material.bindParamMatrix(fxparameterMVP, view * projection);

    cube.draw(context);

    material.end(RCFD);
    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->exec();
}
