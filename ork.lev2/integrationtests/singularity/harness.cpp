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
///////////////////////////////////////////////////////////////////////////////

namespace po = boost::program_options;

#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
}
#endif

static auto the_synth = synth::instance();

SingularityTestApp::SingularityTestApp(int& argc, char** argv)
    : OrkEzQtApp(argc, argv) {
  _hudvp = the_synth->_hudvp;
  startupAudio();
}
SingularityTestApp::~SingularityTestApp() {
  tearDownAudio();
}

singularitytestapp_ptr_t createEZapp(int& argc, char** argv) {

  po::options_description desc("Allowed options");
  desc.add_options()                   //
      ("help", "produce help message") //
      ("hidpi", "hidpi mode");

  po::variables_map vars;
  po::store(po::parse_command_line(argc, argv, desc), vars);
  po::notify(vars);

  if (vars.count("help")) {
    std::cout << desc << "\n";
    exit(0);
  }
  if (vars.count("hidpi")) {
#if defined(__APPLE__)
    ork::lev2::_macosUseHIDPI = true;
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  // boot up debug HUD
  //////////////////////////////////////////////////////////////////////////////
  static auto& qti = qtinit(argc, argv);
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  auto qtapp  = std::make_shared<SingularityTestApp>(qti._argc, qti._argvp);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
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
    DB->Reset();
    DB->copyCameras(*cameras);
    qtapp->_hudvp->onUpdateThreadTick(updata);
    DrawableBuffer::releaseFromWrite(DB);
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
    qtapp->_hudvp->Draw(drwev);
    mtxi->PopUIMatrix();
    context->endFrame();
    ////////////////////////////////////////////////////
    DrawableBuffer::releaseFromRead(DB);
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([=](int w, int h) { //
    // printf("GOTRESIZE<%d %d>\n", w, h);
    qtapp->_hudvp->SetSize(w, h);
  });
  //////////////////////////////////////////////////////////
  const int64_t trackMAX = (4095 << 16);
  qtapp->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool isalt  = ev->mbALT;
    bool isctrl = ev->mbCTRL;
    switch (ev->miEventCode) {
      case ui::UIEV_KEY:
      case ui::UIEV_KEY_REPEAT:
        switch (ev->miKeyCode) {
          case 'p':
            the_synth->_hudpage = (the_synth->_hudpage + 1) % 2;
            break;
          case '-': {
            int64_t amt         = isalt ? 100 : (isctrl ? 1 : 10);
            the_synth->_oswidth = std::clamp(the_synth->_oswidth - amt, int64_t(0), int64_t(4095));
            break;
          }
          case '=': {
            int64_t amt         = isalt ? 100 : (isctrl ? 1 : 10);
            the_synth->_oswidth = std::clamp(the_synth->_oswidth + amt, int64_t(0), int64_t(4095));
            break;
          }
          case '[': {
            float amt             = isalt ? 0.1 : (isctrl ? 0.001 : 0.01);
            the_synth->_ostriglev = std::clamp(the_synth->_ostriglev - amt, -1.0f, 1.0f);
            break;
          }
          case ']': {
            float amt             = isalt ? 0.1 : (isctrl ? 0.001 : 0.01);
            the_synth->_ostriglev = std::clamp(the_synth->_ostriglev + amt, -1.0f, 1.0f);
            break;
          }
          case '\\': {
            the_synth->_ostrigdir = !the_synth->_ostrigdir;
            break;
          }
          default:
            break;
        }
        break;
      default:
        return qtapp->_hudvp->HandleUiEvent(ev);
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  return qtapp;
}
//////////////////////////////////////////////////////////////////////////////
