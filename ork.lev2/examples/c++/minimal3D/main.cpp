////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/primitives.inl>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;

struct Resources {

  Resources(Context* ctx){
    ///////////////////////////////////////////////////
    // init material
    ///////////////////////////////////////////////////
    _material = std::make_shared<FreestyleMaterial>();
    _material->gpuInit(ctx, "orkshader://solid");
    _fxtechnique    = _material->technique("vtxcolor");
    _fxparameterMVP = _material->param("MatMVP");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", _fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", _fxparameterMVP);
    _material->_rasterstate.SetCullTest(ECULLTEST_PASS_FRONT);
    ///////////////////////////////////////////////////
    // init frustum primitive
    ///////////////////////////////////////////////////
    _frustum_prim = std::make_shared<primitives::FrustumPrimitive>();
    _frustum_prim->_colorTop    = fvec3(.5, 1, .5);
    _frustum_prim->_colorBottom = fvec3(.5, 0, .5);
    _frustum_prim->_colorFront  = fvec3(.5, .5, 1);
    _frustum_prim->_colorBack   = fvec3(.5, .5, 0);
    _frustum_prim->_colorLeft   = fvec3(0, .5, .5);
    _frustum_prim->_colorRight  = fvec3(1, .5, .5);
    auto frus_p = ctx->MTXI()->Persp(45.0, 1.0f, .1, 3);
    auto frus_v = ctx->MTXI()->LookAt(fvec3(0, 0, -1), fvec3(0, 0, 0), fvec3(0, 1, 0));
    _frustum_prim->_frustum.set(frus_v, frus_p);
    _frustum_prim->gpuInit(ctx);
  }

  freestyle_mtl_ptr_t _material;
  primitives::frustum_ptr_t _frustum_prim;
  const FxShaderTechnique* _fxtechnique = nullptr;
  const FxShaderParam* _fxparameterMVP  = nullptr;
};

using resources_ptr_t = std::shared_ptr<Resources>;

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto qtapp  = OrkEzApp::create(init_data);
  auto qtwin                           = qtapp->_mainWindow;
  auto gfxwin                          = qtwin->_gfxwin;
  resources_ptr_t resources;
  //////////////////////////////////////////////////////////
  Timer timer;
  timer.Start();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    resources = std::make_shared<Resources>(ctx);
    ///////////////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context = drwev->GetTarget();
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
    resources->_material->begin(resources->_fxtechnique, RCFD);
    resources->_material->bindParamMatrix(resources->_fxparameterMVP, fmtx4::multiply_ltor(view,projection));
    resources->_frustum_prim->renderEML(context);
    resources->_material->end(RCFD);
    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  qtapp->onGpuExit([&](Context* ctx) {
    resources = nullptr;
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->mainThreadLoop();
}
