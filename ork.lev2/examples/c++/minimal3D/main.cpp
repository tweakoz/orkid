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
using vtx_t = ork::lev2::primitives::vtx_t;

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
  cube._colorTop    = fvec4::Red();
  cube._colorBottom = fvec4::Red();
  cube._colorFront  = fvec4::Green();
  cube._colorBack   = fvec4::Green();
  cube._colorLeft   = fvec4::Blue();
  cube._colorRight  = fvec4::Blue();
  //////////////////////////////////////////////////////////
  timer.Start();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "orkshader://solid");
    fxtechnique    = material.technique("debuguv");
    fxparameterMVP = material.param("MatMVP");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    material._rasterstate.SetCullTest(ECULLTEST_PASS_FRONT);
    auto& vtxbuf = GfxEnv::GetRef().GetSharedDynamicVB2();
    cube.gpuInit(ctx, vtxbuf);
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
