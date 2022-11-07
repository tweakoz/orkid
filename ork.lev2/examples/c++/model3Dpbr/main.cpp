////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <iostream>

#include <ork/application/application.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::deferrednode;
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets

struct Instance {
  fvec3 _curpos;
  fvec3 _curaxis;
  float _curangle = 0.0f;
  fvec3 _target;
  fvec3 _targetaxis;
  float _targetangle = 0.0f;
  float _timeout     = 0.0f;
};

using instances_t       = std::vector<Instance>;

///////////////////////////////////////////////////////////////////

struct GpuResources {

  GpuResources(Context* ctx, bool use_forward){
    _renderer       = std::make_shared<DefaultRenderer>();
    _lightmgr       = std::make_shared<LightManager>(_lmd);

    _camlut = std::make_shared<CameraDataLut>();
    _camdata = std::make_shared<CameraData>();
    (*_camlut)["spawncam"] = _camdata;

    _instanced_drawable = std::make_shared<InstancedModelDrawable>();

  //////////////////////////////////////////////////////////
  // initialize compositor (necessary for PBR models)
  //  use a deferredPBR compositing node
  //  which does all the gbuffer and lighting passes
  //////////////////////////////////////////////////////////

    _compositordata = std::make_shared<CompositingData>();
    if(use_forward){
      _compositordata->presetForwardPBR();
    }
    else{
      _compositordata->presetDeferredPBR();
    }
    _compositordata->mbEnable = true;
    auto nodetek             = _compositordata->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
    auto outpnode            = nodetek->tryOutputNodeAs<ScreenOutputCompositingNode>();
    // outpnode->setSuperSample(4);
    _compositorimpl = _compositordata->createImpl();
    _compositorimpl->bindLighting(_lightmgr.get());

    _TOPCPD = std::make_shared<lev2::CompositingPassData>();
    _TOPCPD->addStandardLayers();
    _instances          = std::make_shared<instances_t>();

     ctx->debugPushGroup("main.onGpuInit");
    _modelasset = asset::AssetManager<XgmModelAsset>::load("data://tests/pbr1/pbr1");
    _renderer->setContext(ctx);

    _instanced_drawable->bindModel(_modelasset->getSharedModel());

    constexpr size_t KNUMINSTANCES = 30;

    _instanced_drawable->resize(KNUMINSTANCES);
    _instanced_drawable->gpuInit(ctx);

    for (int i = 0; i < KNUMINSTANCES; i++) {
      Instance inst;
      _instances->push_back(inst);
    }
     ctx->debugPopGroup();

  }
  instanced_modeldrawable_ptr_t _instanced_drawable;
  renderer_ptr_t _renderer;
  LightManagerData _lmd;
  lightmanager_ptr_t _lightmgr;
  compositingpassdata_ptr_t _TOPCPD;
  compositorimpl_ptr_t _compositorimpl;
  compositordata_ptr_t _compositordata;
  std::shared_ptr<instances_t> _instances;
  lev2::xgmmodelassetptr_t _modelasset; // retain model
  cameradata_ptr_t _camdata;
  cameradatalut_ptr_t _camlut;
};

///////////////////////////////////////////////////////////////////

int main(int argc, char** argv,char** envp) {

  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);

  auto desc = init_data->commandLineOptions("model3dpbr example Options");
  desc->add_options() //
    ("help", "produce help message") //
    ("forward", po::bool_switch()->default_value(false), "forward renderer");

  auto vars        = *init_data->parse();

  if (vars.count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }

  bool use_forward = vars["forward"].as<bool>();

  init_data->_msaa_samples = use_forward ? 4 : 1;

  auto qtapp  = OrkEzApp::create(init_data);
  auto qtwin              = qtapp->_mainWindow;
  auto gfxwin             = qtwin->_gfxwin;
  std::shared_ptr<GpuResources> gpurec;
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {

    gpurec = std::make_shared<GpuResources>(ctx,use_forward);
  });
  //////////////////////////////////////////////////////////
  // update handler (called on update thread)
  //  it will never be called before onGpuInit() is complete...
  //////////////////////////////////////////////////////////
  ork::Timer timer;
  timer.Start();
  auto dbufcontext = std::make_shared<DrawBufContext>();
  qtapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase    = abstime * PI2 * 0.1f;
    float distance = 10.0f;
    auto eye       = fvec3(sinf(phase), 1.0f, -cosf(phase)) * distance;
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    gpurec->_camdata->Lookat(eye, tgt, up);
    gpurec->_camdata->Persp(1, 20.0, 45.0);
    ///////////////////////////////////////
    auto DB = dbufcontext->acquireForWriteLocked();
    DB->Reset();
    DB->copyCameras(*gpurec->_camlut);
    auto layer = DB->MergeLayer("Default");
    ////////////////////////////////////////
    // animate and enqueue all instances
    ////////////////////////////////////////

    auto drawable = gpurec->_instanced_drawable;
    auto instdata = drawable->_instancedata;

    int index = 0;
    for (auto& inst : *gpurec->_instances) {
      //auto drawable = static_cast<Drawable*>(inst._drawable);
      fvec3 delta   = inst._target - inst._curpos;
      inst._curpos += delta.normalized() * dt * 1.0;

      delta         = inst._targetaxis - inst._curaxis;
      inst._curaxis = (inst._curaxis + delta.normalized() * dt * 0.1).normalized();
      inst._curangle += (inst._targetangle - inst._curangle) * dt * 0.1;

      if (inst._timeout < abstime) {
        inst._timeout  = abstime + float(rand() % 255) / 64.0;
        inst._target.x = (float(rand() % 255) / 2.55) - 50;
        inst._target.y = (float(rand() % 255) / 2.55) - 50;
        inst._target.z = (float(rand() % 255) / 2.55) - 50;
        inst._target *= (4.5f/50.0f);

        fvec3 axis;
        axis.x            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.y            = (float(rand() % 255) / 255.0f) - 0.5f;
        axis.z            = (float(rand() % 255) / 255.0f) - 0.5f;
        inst._targetaxis  = axis.normalized();
        inst._targetangle = PI2 * (float(rand() % 255) / 255.0f) - 0.5f;
      }

      fquat q;
      q.fromAxisAngle(fvec4(inst._curaxis, inst._curangle));
      instdata->_worldmatrices[index++].compose(inst._curpos, q, 0.3f);
    }
    DrawQueueXfData ident;
    drawable->enqueueOnLayer(ident, *layer);
    ////////////////////////////////////////
    dbufcontext->releaseFromWriteLocked(DB);
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto DB = dbufcontext->acquireForReadLocked();
    if (nullptr == DB)
      return;

    float time = timer.SecsSinceStart();
    auto context = drwev->GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = gpurec->_compositorimpl;
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    RCFD.setUserProperty("time"_crc, time);
    RCFD.setUserProperty("pbr_model"_crc, 1);
    context->pushRenderContextFrameData(&RCFD);
    auto fbi  = context->FBI();  // FrameBufferInterface
    auto fxi  = context->FXI();  // FX Interface
    auto mtxi = context->MTXI(); // matrix Interface
    auto gbi  = context->GBI();  // GeometryBuffer Interface
    ///////////////////////////////////////
    // compositor setup
    ///////////////////////////////////////
    lev2::UiViewportRenderTarget rt(nullptr);
    auto tgtrect           = context->mainSurfaceRectAtOrigin();
    gpurec->_TOPCPD->_time = time;
    gpurec->_TOPCPD->_irendertarget = &rt;
    gpurec->_TOPCPD->SetDstRect(tgtrect);
    gpurec->_compositorimpl->pushCPD(*gpurec->_TOPCPD);
    ///////////////////////////////////////
    // Draw!
    ///////////////////////////////////////
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    fbi->setViewport(tgtrect);
    fbi->setScissor(tgtrect);
    context->beginFrame();
    FrameRenderer framerenderer(RCFD, [&]() {});
    CompositorDrawData drawdata(framerenderer);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(gpurec->_renderer.get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawableBuffer*>(DB);
    drawdata._cimpl = gpurec->_compositorimpl;
    gpurec->_compositorimpl->assemble(drawdata);
    gpurec->_compositorimpl->composite(drawdata);
    gpurec->_compositorimpl->popCPD();
    context->popRenderContextFrameData();
    context->endFrame();
    dbufcontext->releaseFromReadLocked(DB);
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) {
    //
    gpurec->_compositorimpl->compositingContext().Resize(w, h);
  });
  qtapp->onGpuExit([&](Context* ctx) {
    gpurec = nullptr;
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->mainThreadLoop();
}
