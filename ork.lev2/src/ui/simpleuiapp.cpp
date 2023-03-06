////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#include <boost/program_options.hpp>
#include <iostream>
#include <ork/lev2/aud/singularity/hud.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/simpleuiapp.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
///////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
extern int G_MSAASAMPLES;
} // namespace ork::lev2
#endif
///////////////////////////////////////////////////////////////////////////////
namespace ork::ui{
///////////////////////////////////////////////////////////////////////////////
SimpleUiApp::SimpleUiApp(appinitdata_ptr_t initdata)
    : OrkEzApp(initdata) {
}
///////////////////////////////////////////////////////////////////////////////
SimpleUiApp::~SimpleUiApp() {
}
///////////////////////////////////////////////////////////////////////////////
simpleuiapp_ptr_t createSimpleUiApp(appinitdata_ptr_t initdata ) {
#if defined(__APPLE__)
  _macosUseHIDPI = false;
  G_MSAASAMPLES  = 1;
#endif

  //////////////////////////////////////////////////////////////////////////////
  // boot up debug HUD
  //////////////////////////////////////////////////////////////////////////////

  auto ezapp                      = std::make_shared<SimpleUiApp>(initdata);
  auto ezwin                      = ezapp->_mainWindow;
  auto appwin                     = ezwin->_appwin;
  auto uicontext                  = ezapp->_uicontext;
  appwin->mRootWidget->_uicontext = uicontext.get();
  //////////////////////////////////////////////////////////
  // create references to various items scoped by ezapp
  //////////////////////////////////////////////////////////
  auto renderer = ezapp->_vars.makeSharedForKey<DefaultRenderer>("renderer");
  auto lmd      = ezapp->_vars.makeSharedForKey<LightManagerData>("lmgrdata");
  auto lightmgr = ezapp->_vars.makeSharedForKey<LightManager>("lmgr", *lmd);
  auto compdata = ezapp->_vars.makeSharedForKey<CompositingData>("compdata");
  auto material = ezapp->_vars.makeSharedForKey<FreestyleMaterial>("material");
  auto CPD      = ezapp->_vars.makeSharedForKey<CompositingPassData>("CPD");
  auto cameras  = ezapp->_vars.makeSharedForKey<CameraDataLut>("cameras");
  auto camdata  = ezapp->_vars.makeSharedForKey<CameraData>("camdata");
  //////////////////////////////////////////////////////////
  compdata->presetUnlit();
  compdata->mbEnable  = true;
  auto nodetek        = compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
  auto outpnode       = nodetek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
  auto compositorimpl = compdata->createImpl();
  compositorimpl->bindLighting(lightmgr.get());
  CPD->addStandardLayers();
  (*cameras)["spawncam"] = camdata;
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([=](lev2::Context* ctx) {
    renderer->setContext(ctx);
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
  auto dbufcontext = std::make_shared<DrawBufContext>();
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    ///////////////////////////////////////
    auto DB = dbufcontext->acquireForWriteLocked();
    if (DB) {
      DB->Reset();
      DB->copyCameras(*cameras);
      // ezapp->_ezviewport->onUpdateThreadTick(updata);
      dbufcontext->releaseFromWriteLocked(DB);
    }
  });
  //////////////////////////////////////////////////////////
  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    ////////////////////////////////////////////////
    auto DB = dbufcontext->acquireForReadLocked();
    if (nullptr == DB)
      return;
    ////////////////////////////////////////////////
    auto context = drwev->GetTarget();
    auto fbi     = context->FBI();  // FrameBufferInterface
    auto fxi     = context->FXI();  // FX Interface
    auto mtxi    = context->MTXI(); // FX Interface
    fbi->SetClearColor(fvec4(0.0, 0.0, 0.1, 1));
    ////////////////////////////////////////////////////
    // draw the synth HUD
    ////////////////////////////////////////////////////
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = compositorimpl;
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    context->pushRenderContextFrameData(&RCFD);
    lev2::UiViewportRenderTarget rt(nullptr);
    auto tgtrect        = context->mainSurfaceRectAtOrigin();
    CPD->_irendertarget = &rt;
    CPD->SetDstRect(tgtrect);
    compositorimpl->pushCPD(*CPD);
    context->beginFrame();
    mtxi->PushUIMatrix();
    ezapp->_uicontext->draw(drwev);
    mtxi->PopUIMatrix();
    context->endFrame();
    ////////////////////////////////////////////////////
    dbufcontext->releaseFromReadLocked(DB);
  });
  //////////////////////////////////////////////////////////
  ezapp->onResize([=](int w, int h) { //
    // printf("GOTRESIZE<%d %d>\n", w, h);
    ezapp->_ezviewport->SetSize(w, h);
    ezapp->_uicontext->_top->SetSize(w, h);
    // ezapp->_ezviewport->_topLayoutGroup->SetSize(w, h);
  });
  //////////////////////////////////////////////////////////
  ezapp->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool isalt  = ev->mbALT;
    bool isctrl = ev->mbCTRL;
    switch (ev->_eventcode) {
      case ui::EventCode::KEY_DOWN:
      case ui::EventCode::KEY_REPEAT:
        switch (ev->miKeyCode) {
          case 'p':
            break;
          default:
            break;
        }
        break;
      default:
        //OrkAssert(false);
        // return uicontext->handleEvent(ev);
        // return ezapp->_ezviewport->HandleUiEvent(ev);
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  return ezapp;
}
//////////////////////////////////////////////////////////////////////////////
SimpleUiViewport::SimpleUiViewport()
    : ui::Viewport("TEST", 0, 0, 1280, 720, fvec3::Red(), 1.0) {
}
///////////////////////////////////////////////////////////////////////////////

void SimpleUiViewport::DoDraw(ui::drawevent_constptr_t drwev) {
  drawChildren(drwev);
}
void SimpleUiViewport::onUpdateThreadTick(ui::updatedata_ptr_t updata) {
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::ui{
