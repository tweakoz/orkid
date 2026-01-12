#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/vr/vr.h>
#include <boost/program_options.hpp>
#include <ork/kernel/environment.h>
#include <ork/util/logger.h>
#include <ork/profiling.inl>

using namespace std::string_literals;

namespace ork::lev2 {
static logchannel_ptr_t logchan_ezapp = logger()->getChannel("EZAPP2");
///////////////////////////////////////////////////////////////////////////////
EzTopWidget::EzTopWidget(EzMainWin* mainwin)
    : ui::Group("ezviewport", 1, 1, 1, 1)
    , _mainwin(mainwin) {
  _geometry._w = 1;
  _geometry._h = 1;
  _initstate.store(0);
  lev2::DrawQueue::ClearAndSyncWriters();
  _mainwin->_render_timer.Start();
  _mainwin->_render_prevtime = _mainwin->_render_timer.SecsSinceStart();
}
/////////////////////////////////////////////////
void EzTopWidget::_doGpuInit(ork::lev2::Context* pTARG) {

  pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  _initstate.store(1);
}
/////////////////////////////////////////////////
void EzTopWidget::enableUiDraw() {
  auto ezapp = (OrkEzApp*)OrkEzAppBase::get();
  /////////////////////////////////////////////////////////////////////
  auto lmd         = ezapp->_vars->makeSharedForKey<LightManagerData>("lmgrdata");
  auto lightmgr    = ezapp->_vars->makeSharedForKey<LightManager>("lmgr", lmd);
  auto compdata    = ezapp->_vars->makeSharedForKey<CompositingData>("compdata");
  auto CPD         = ezapp->_vars->makeSharedForKey<CompositingPassData>("CPD");
  auto dbufcontext = ezapp->_vars->makeSharedForKey<DrawQueueContext>("dbufcontext");
  auto cameras     = ezapp->_vars->makeSharedForKey<CameraDataLut>("cameras");
  auto camdata     = ezapp->_vars->makeSharedForKey<CameraData>("camdata");
  auto rcfd        = ezapp->_vars->makeSharedForKey<RenderContextFrameData>("RCFD");
  /////////////////////////////////////////////////////////////////////
  compdata->presetUnlit();
  compdata->mbEnable    = true;
  auto nodetek          = compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
  auto outpnode         = nodetek->tryOutputNodeAs<ScreenOutputCompositingNode>();
  auto compositorimpl   = compdata->createImpl();
  compositorimpl->_name = "EzTopWidget::compositorimpl";
  compositorimpl->bindLighting(lightmgr);
  CPD->addStandardLayers();
  (*cameras)["spawncam"] = camdata;
  /////////////////////////////////////////////////////////////////////
  auto update_buffer = std::make_shared<AcquiredDrawQueueForUpdate>();
  auto draw_buffer   = std::make_shared<AcquiredDrawQueueForRendering>();
  /////////////////////////////////////////////////////////////////////
  ezapp->_mainWindow->_onUpdateInternal = [=](ui::updatedata_ptr_t updata) {
    auto DB = dbufcontext->acquireForWriteLocked();
    if (DB) {
      update_buffer->_DB = DB;
      DB->Reset();
      DB->copyCameras(*cameras);
      //   ezapp->_eztopwidget->onUpdateThreadTick(updata);
      dbufcontext->releaseFromWriteLocked(DB);
    }
  };
  /////////////////////////////////////////////////////////////////////
  ezapp->onDraw([=](ui::drawevent_constptr_t drwev) {
    ////////////////////////////////////////////////
    auto DB = dbufcontext->acquireForReadLocked();
    ////////////////////////////////////////////////
    auto context = drwev->GetTarget();
    auto fbi     = context->FBI();  // FrameBufferInterface
    auto fxi     = context->FXI();  // FX Interface
    auto mtxi    = context->MTXI(); // FX Interface
    ////////////////////////////////////////////////////
    fbi->SetClearColor(fvec4(0.0, 0.0, 0.1, 1));
    //fbi->Clear(fvec4(1.0, 0.0, 1.0, 1), 1);
    ////////////////////////////////////////////////////
    rcfd->_name = "EzTopWidget::rcfd";
    rcfd->pushCompositor(compositorimpl);
    if (DB)
      rcfd->setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    rcfd->_target = context;
    context->pushRenderContextFrameData(rcfd);
    draw_buffer->_DB        = DB;
    draw_buffer->_RCFD      = rcfd;
    auto mutable_drwev      = std::const_pointer_cast<ui::DrawEvent>(drwev);
    mutable_drwev->_acqdbuf = draw_buffer;
    ////////////////////////////////////////////////////
    if(ezapp->_mainWindow->_onGpuPreFrame){
      ezapp->_mainWindow->_onGpuPreFrame(context);
    }
    ////////////////////////////////////////////////////
    lev2::UiViewportRenderTarget rt(nullptr);
    auto tgtrect        = context->mainSurfaceRectAtOrigin();
    CPD->_irendertarget = &rt;
    CPD->SetDstRect(tgtrect);
    compositorimpl->pushCPD(*CPD);
    //context->beginFrame();
    mtxi->PushUIMatrix();
    ezapp->_uicontext->draw(drwev);
    mtxi->PopUIMatrix();
    compositorimpl->popCPD();

    if(ezapp->_mainWindow->_onGpuPostFrame){
      ezapp->_mainWindow->_onGpuPostFrame(context);
    }

    if(ezapp->_movie_record_frame_lambda){
      ezapp->_movie_record_frame_lambda( context );
    }

    //context->endFrame();
    rcfd->popCompositor();
    ////////////////////////////////////////////////////
    if (DB)
      dbufcontext->releaseFromReadLocked(DB);
  });
}
/////////////////////////////////////////////////
void EzTopWidget::DoDraw(ui::drawevent_constptr_t drwev) {
  static int frame_counter = 0;
  logchan_ezapp->log("[EzTopWidget::DoDraw] ENTER frame %d", frame_counter);
  //////////////////////////////////////////////////////
  // ensure onUpdateInit called before onGpuInit!
  //////////////////////////////////////////////////////
  auto ezapp = (OrkEzApp*)OrkEzAppBase::get();
  if (not ezapp->checkAppState(KAPPSTATEFLAG_UPDRUNNING))
    return;
  ///////////////////////////
  drwev->GetTarget()->makeCurrentContext();
  ///////////////////////////
  while (ezapp->_rthreadq->Process()) {
  }
  ///////////////////////////
  if (_mainwin->_onDraw) {
    EASY_BLOCK("EzTopWidget drawcontent", profiler::colors::Red);
    auto ctx = drwev->GetTarget();
    int swap_w = 0, swap_h = 0;
    ctx->FBI()->querySwapchainSize(swap_w, swap_h);
    void* swap_ptr = ctx->FBI()->querySwapchainPtr();
    ctx->beginFrame();
    if(ctx->FBI()->_main_rtg){
      _mainwin->_onDraw(drwev);
    }
    logchan_ezapp->log("[EzTopWidget::DoDraw] endFrame frame %d", frame_counter);
    ctx->endFrame();
    EASY_END_BLOCK;
    EASY_BLOCK("EzTopWidget swap", 0xffc04000);
    ctx->swapBuffers(ctx->mCtxBase);
    EASY_END_BLOCK;
    ezapp->_render_count.fetch_add(1);
  }
  ///////////////////////////
  double this_time           = _mainwin->_render_timer.SecsSinceStart();
  _mainwin->_render_prevtime = this_time;
  if (this_time >= logchan_ezapp->_status_interval) {
    double FPS = _mainwin->_render_state_numiters / this_time;
    logchan_ezapp->status("FPS","<%g>", FPS);
    _mainwin->_render_state_numiters = 0.0;
    _mainwin->_render_timer.Start();
  } else {
    _mainwin->_render_state_numiters += 1.0;
  }
  logchan_ezapp->log("[EzTopWidget::DoDraw] EXIT frame %d", frame_counter);
  frame_counter++;
}
/////////////////////////////////////////////////
void EzTopWidget::_doOnResized() {
  logchan_ezapp->status("topwidget resize", "<%d %d>", width(), height());
  if (_mainwin->_onResize) {
    _mainwin->_onResize(width(), height());
  }
  _topLayoutGroup->SetSize(width(), height());
}
/////////////////////////////////////////////////
ui::HandlerResult EzTopWidget::DoOnUiEvent(ui::event_constptr_t ev) {
  if (_mainwin->_onUiEvent) {
    auto hacked_event      = std::make_shared<ui::Event>();
    *hacked_event          = *ev;
    hacked_event->_vpdim.x = width();
    hacked_event->_vpdim.y = height();
    return _mainwin->_onUiEvent(hacked_event);
  } else {
    return ui::HandlerResult();
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
