////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <iostream>

#include <ork/application/application.h>
#include <ork/kernel/datacache.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.h>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::pbr::deferrednode;

int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);

  auto desc = init_data->commandLineOptions("model3dpbr example Options");
  desc->add_options()                  //
      ("help", "produce help message") //
      ("msaa", po::value<int>()->default_value(1), "msaa samples(*1,4,9,16,25)")
      ("ssaa", po::value<int>()->default_value(1), "ssaa samples(*1,4,9,16,25)")
      ("forward", po::bool_switch()->default_value(false), "forward renderer");

  auto vars = *init_data->parse();

  if (vars.count("help")) {
    std::cout << (*desc) << "\n";
    exit(0);
  }
  init_data->_msaa_samples = vars["msaa"].as<int>();
  init_data->_ssaa_samples = vars["ssaa"].as<int>();

  bool use_forward = vars["forward"].as<bool>();

  auto qtapp        = OrkEzApp::create(init_data);
  auto qtwin        = qtapp->_mainWindow;
  auto gfxwin       = qtwin->_gfxwin;
  Texture* envlight = nullptr;
  hfdrawableinstptr_t _terrainInst;
  auto _terrainData = std::make_shared<TerrainDrawableData>();
  drawable_ptr_t _terrainDrawable;
  DrawableCache dcache;
  //////////////////////////////////////////////////////////
  // initialize compositor data
  //  use a deferredPBR compositing node
  //  which does all the gbuffer and lighting passes
  //////////////////////////////////////////////////////////
  auto renderer = std::make_shared<DefaultRenderer>();
  auto lmd      = std::make_shared<LightManagerData>();
  auto lightmgr = std::make_shared<LightManager>(*lmd);
  auto cameras = std::make_shared<CameraDataLut>();
  auto camdata = std::make_shared<CameraData>();
  compositingpassdata_ptr_t TOPCPD;
  compositorimpl_ptr_t compositorimpl;
  compositordata_ptr_t compositordata;
  //////////////////////////////////////////////////////////
  // gpuInit handler, called once on main(rendering) thread
  //  at startup time
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {

    compositordata = std::make_shared<CompositingData>();
    if(use_forward)
      compositordata->presetForwardPBR();
    else
      compositordata->presetDeferredPBR();

    compositordata->mbEnable     = true;
    auto nodetek                = compositordata->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
    auto rendnode               = nodetek->tryRenderNodeAs<pbr::deferrednode::DeferredCompositingNodePbr>();
    auto pbrcommon              = rendnode->_pbrcommon;
    pbrcommon->_depthFogDistance = 4000.0f;
    pbrcommon->_depthFogPower    = 5.0f;
    ///////////////////////////////////////
    // compositor instance
    ///////////////////////////////////////
    compositorimpl = compositordata->createImpl();
    compositorimpl->bindLighting(lightmgr.get());
    TOPCPD = std::make_shared<lev2::CompositingPassData>();
    TOPCPD->addStandardLayers();
    (*cameras)["spawncam"] = camdata;
    //////////////////////////////////////////////////////////
    _terrainData->_rock1 = fvec3(1, 1, 1);
    _terrainData->_writeHmapPath("src://terrain/testhmap2_2048.png");

    _terrainDrawable          = dcache.fetch(_terrainData);
    _terrainInst = _terrainDrawable->GetUserDataB().getShared<TerrainDrawableInst>();
    _terrainInst->_worldHeight = 5000.0f;
    _terrainInst->_worldSizeXZ = 8192.0f;


    ctx->debugPushGroup("main.onGpuInit");
    renderer->setContext(ctx);
    ctx->debugPopGroup();
  });
  //////////////////////////////////////////////////////////
  // onUpdateInit (always called after onGpuInit() is complete...)
  //////////////////////////////////////////////////////////
  auto dbufcontext = std::make_shared<DrawBufContext>();
  qtapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    double dt      = updata->_dt;
    double abstime = updata->_abstime;
    ///////////////////////////////////////
    // compute camera data
    ///////////////////////////////////////
    float phase = abstime * PI2 * 0.01f;
    fvec3 eye(245, 150, -330);
    auto tgt = eye + fvec3(sinf(phase), -0.1f, -cosf(phase));
    fvec3 up(0, 1, 0);
    camdata->Lookat(eye, tgt, up);
    camdata->Persp(0.1, 6000.0, 45.0);
    ///////////////////////////////////////
    // enqueue terrain (and whole frame)
    ///////////////////////////////////////
    auto DB = dbufcontext->acquireForWriteLocked();
    DB->Reset();
    DB->copyCameras(*cameras);
    auto layer = DB->MergeLayer("Default");
    DrawQueueXfData _terrainXform;
    _terrainDrawable->enqueueOnLayer(_terrainXform, *layer);
    dbufcontext->releaseFromWriteLocked(DB);
  });
  //////////////////////////////////////////////////////////
  // draw handler (called on main(rendering) thread)
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto DB = dbufcontext->acquireForReadLocked();
    if (nullptr == DB)
      return;

    auto context = drwev->GetTarget();
 
    renderer.get()->setContext(context);

    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = compositorimpl;
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    context->pushRenderContextFrameData(&RCFD);
    auto fbi = context->FBI(); // FrameBufferInterface
    ///////////////////////////////////////
    // compositor setup
    ///////////////////////////////////////
    lev2::UiViewportRenderTarget rt(nullptr);
    float TARGW            = context->mainSurfaceWidth();
    float TARGH            = context->mainSurfaceHeight();
    auto tgtrect           = ViewportRect(0, 0, TARGW, TARGH);
    TOPCPD->_irendertarget = &rt;
    TOPCPD->SetDstRect(tgtrect);
    compositorimpl->pushCPD(*TOPCPD);
    ///////////////////////////////////////
    // render !
    ///////////////////////////////////////
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    fbi->setViewport(tgtrect);
    fbi->setScissor(tgtrect);
    context->beginFrame();
    FrameRenderer framerenderer(RCFD, [&]() {});
    CompositorDrawData drawdata(framerenderer);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(renderer.get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawableBuffer*>(DB);
    drawdata._cimpl = compositorimpl;
    compositorimpl->assemble(drawdata);
    compositorimpl->composite(drawdata);
    compositorimpl->popCPD();
    context->popRenderContextFrameData();
    context->endFrame();
    dbufcontext->releaseFromReadLocked(DB);
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) {
    //
    compositorimpl->compositingContext().Resize(w, h);
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->mainThreadLoop();
}
