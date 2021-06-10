////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#include "harness.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <ork/lev2/aud/singularity/hud.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorForward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/ui/context.h>
///////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
}
#endif
///////////////////////////////////////////////////////////////////////////////
UiTestApp::UiTestApp(int& argc, char** argv)
    : OrkEzQtApp(argc, argv,QtAppInitData()) {
}
///////////////////////////////////////////////////////////////////////////////
UiTestApp::~UiTestApp() {
}
///////////////////////////////////////////////////////////////////////////////
uitestapp_ptr_t createEZapp(int& argc, char** argv) {

  //////////////////////////////////////////////////////////////////////////////
  // boot up debug HUD
  //////////////////////////////////////////////////////////////////////////////
  static auto& qti = qtinit(argc, argv);
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  auto qtapp                      = std::make_shared<UiTestApp>(qti._argc, qti._argvp);
  auto qtwin                      = qtapp->_mainWindow;
  auto gfxwin                     = qtwin->_gfxwin;
  auto uicontext                  = qtapp->_uicontext;
  gfxwin->mRootWidget->_uicontext = uicontext.get();
  //////////////////////////////////////////////////////////
  // create references to various items scoped by qtapp
  //////////////////////////////////////////////////////////
  auto renderer = qtapp->_vars.makeSharedForKey<DefaultRenderer>("renderer");
  auto lmd      = qtapp->_vars.makeSharedForKey<LightManagerData>("lmgrdata");
  auto lightmgr = qtapp->_vars.makeSharedForKey<LightManager>("lmgr", *lmd);
  auto compdata = qtapp->_vars.makeSharedForKey<CompositingData>("compdata");
  auto material = qtapp->_vars.makeSharedForKey<FreestyleMaterial>("material");
  auto CPD      = qtapp->_vars.makeSharedForKey<CompositingPassData>("CPD");
  auto cameras  = qtapp->_vars.makeSharedForKey<CameraDataLut>("cameras");
  auto camdata  = qtapp->_vars.makeSharedForKey<CameraData>("camdata");
  //////////////////////////////////////////////////////////
  compdata->presetForward();
  compdata->mbEnable  = true;
  auto nodetek        = compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
  auto outpnode       = nodetek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
  auto compositorimpl = compdata->createImpl();
  compositorimpl->bindLighting(lightmgr.get());
  CPD->addStandardLayers();
  cameras->AddSorted("spawncam", camdata.get());
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([=](Context* ctx) {
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
  qtapp->onUpdate([=](ui::updatedata_ptr_t updata) {
    ///////////////////////////////////////
    auto DB = DrawableBuffer::acquireForWrite(0);
    if (DB) {
      DB->Reset();
      DB->copyCameras(*cameras);
      // qtapp->_ezviewport->onUpdateThreadTick(updata);
      DrawableBuffer::releaseFromWrite(DB);
    }
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([=](ui::drawevent_constptr_t drwev) {
    ////////////////////////////////////////////////
    auto DB = DrawableBuffer::acquireForRead(7);
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
    qtapp->_uicontext->draw(drwev);
    mtxi->PopUIMatrix();
    context->endFrame();
    ////////////////////////////////////////////////////
    DrawableBuffer::releaseFromRead(DB);
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([=](int w, int h) { //
    // printf("GOTRESIZE<%d %d>\n", w, h);
    qtapp->_ezviewport->SetSize(w, h);
    qtapp->_ezviewport->_topLayoutGroup->SetSize(w, h);
  });
  //////////////////////////////////////////////////////////
  const int64_t trackMAX = (4095 << 16);
  qtapp->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool isalt  = ev->mbALT;
    bool isctrl = ev->mbCTRL;
    switch (ev->_eventcode) {
      case ui::EventCode::KEY:
      case ui::EventCode::KEY_REPEAT:
        switch (ev->miKeyCode) {
          case 'p':
            break;
          default:
            break;
        }
        break;
      default:
        OrkAssert(false);
        // return uicontext->handleEvent(ev);
        // return qtapp->_ezviewport->HandleUiEvent(ev);
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  return qtapp;
}
//////////////////////////////////////////////////////////////////////////////
TestViewport::TestViewport()
    : ui::Viewport("TEST", 0, 0, 1280, 720, fvec3::Red(), 1.0) {
}
