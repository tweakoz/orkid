////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <iostream>

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
    auto fxtechnique    = _material->technique("vtxcolor");
    auto fxparameterMVP = _material->param("MatMVP");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    _material->_rasterstate.SetCullTest(ECullTest::PASS_FRONT);

    ///////////////////////////////////////////////////
    // create RCFD, RCID
    ///////////////////////////////////////////////////

    _RCFD = std::make_shared<RenderContextFrameData>(ctx);
    _RCFD->_renderingmodel = RenderingModel("FORWARD_UNLIT"_crcu);
    _RCID = RenderContextInstData::create(_RCFD);
    _RCID->forceTechnique(fxtechnique);

    ///////////////////////////////////////////////////
    // create simple compositor (needed for pipeline based rendering)
    ///////////////////////////////////////////////////

    _compdata = std::make_shared<CompositingData>();
    _compimpl = std::make_shared<CompositingImpl>(*_compdata);
    _CPD._cameraMatrices = & _cammatrices;
    _RCFD->pushCompositor(_compimpl); // bind compositor to _RCFD

    ///////////////////////////////////////////////////
    // primitive instance is always rendered at origin
    ///////////////////////////////////////////////////

    _RCID->_genMatrix = [] -> fmtx4{
        return fmtx4();
    };

    ///////////////////////////////////////////////////
    auto fxcache = _material->pipelineCache();
    _pipeline = fxcache->findPipeline(*_RCID);
    _pipeline->_params[fxparameterMVP] = "RCFD_Camera_MVP_Mono"_crcsh;
    ///////////////////////////////////////////////////
    // init frustum primitive
    ///////////////////////////////////////////////////
    _frustum_prim = std::make_shared<primitives::FrustumPrimitive>();
    _frustum_prim->_colorTop    = dvec3(.5, 1, .5);
    _frustum_prim->_colorBottom = dvec3(.5, 0, .5);
    _frustum_prim->_colorNear  = dvec3(.5, .5, 1);
    _frustum_prim->_colorFar   = dvec3(.5, .5, 0);
    _frustum_prim->_colorLeft   = dvec3(0, .5, .5);
    _frustum_prim->_colorRight  = dvec3(1, .5, .5);
    dmtx4 frus_p;
    dmtx4 frus_v;
    frus_p.perspective(45.0*DTOR, 1.0f, .1, 3);
    frus_v.lookAt(dvec3(0, 0, -1), dvec3(0, 0, 0), dvec3(0, 1, 0));
    _frustum_prim->_frustum.set(frus_v, frus_p);
    _frustum_prim->gpuInit(ctx);
  }


  compositordata_ptr_t _compdata;
  compositorimpl_ptr_t _compimpl;
  CompositingPassData _CPD;
  CameraMatrices _cammatrices;
  freestyle_mtl_ptr_t _material;
  primitives::frustum_ptr_t _frustum_prim;
  fxpipeline_ptr_t _pipeline;
  rcfd_ptr_t _RCFD; // renderer per/frame data
  rcid_ptr_t _RCID;

};

using resources_ptr_t = std::shared_ptr<Resources>;

int main(int argc, char** argv,char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);

  auto desc = init_data->commandLineOptions("minimal3d example Options");
  desc->add_options()                  //
      ("help", "produce help message") //
      ("msaa", po::value<int>()->default_value(1), "msaa samples(*1,4,9,16,25)");

  auto vars = *init_data->parse();

  if (vars.count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }

  init_data->_msaa_samples = vars["msaa"].as<int>();

  auto ezapp  = OrkEzApp::create(init_data);
  auto ezwin                           = ezapp->_mainWindow;
  auto appwin                          = ezwin->_appwin;
  resources_ptr_t resources;
  //////////////////////////////////////////////////////////
  Timer timer;
  timer.Start();
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([&](Context* ctx) {
    resources = std::make_shared<Resources>(ctx);
    ///////////////////////////////////////////////////
  });
  //////////////////////////////////////////////////////////
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context = drwev->GetTarget();
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
    auto projection = mtxi->Persp(45, aspect, 0.1, 100.0);
    auto view       = mtxi->LookAt(eye, tgt, up);

    ///////////////////////////////////////
    // assign view/projection matrices
    //  to compositor (via CPD's mono cameramatrices)
    ///////////////////////////////////////

    resources->_cammatrices.setCustomView(view);
    resources->_cammatrices.setCustomProjection(projection);

    ///////////////////////////////////////
    // set clear color
    ///////////////////////////////////////

    fbi->SetClearColor(fvec4(0, 0, 0, 1));

    ///////////////////////////////////////
    // render frame
    ///////////////////////////////////////

    context->beginFrame();
    // push compositing pass data
    resources->_compimpl->pushCPD(resources->_CPD);
    {
      resources->_pipeline->wrappedDrawCall(*resources->_RCID,[=](){
        resources->_frustum_prim->renderEML(context);
      });
    }
    // pop compositing pass data
    resources->_compimpl->popCPD();
    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  ezapp->onGpuExit([&](Context* ctx) {
    resources = nullptr;
  });
  //////////////////////////////////////////////////////////
  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return ezapp->mainThreadLoop();
}
