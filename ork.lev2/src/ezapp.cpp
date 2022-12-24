#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <boost/program_options.hpp>
#include <ork/kernel/environment.h>
#include <ork/util/logger.h>

using namespace std::string_literals; 

namespace ork::imgui {
void initModule(appinitdata_ptr_t initdata) {
  initdata->_imgui = true;
}
} // namespace ork::imgui

namespace ork::lev2 {
extern bool g_allow_HIDPI;
void ClassInit();
void GfxInit(const std::string& gfxlayer);
constexpr uint64_t KAPPSTATEFLAG_UPDRUNNING = 1 << 0;
constexpr uint64_t KAPPSTATEFLAG_JOINED     = 1 << 1;

static logchannel_ptr_t logchan_ezapp = logger()->createChannel("ezapp",fvec3(0.7,0.7,0.9));

////////////////////////////////////////////////////////////////////////////////
ezappctx_ptr_t EzAppContext::get(appinitdata_ptr_t initdata) {
  if(nullptr==initdata){
    initdata = std::make_shared<AppInitData>();
  }
  initModule(initdata);
  static auto app  = std::shared_ptr<EzAppContext>(new EzAppContext(initdata));
  return app;
}
////////////////////////////////////////////////////////////////////////////////
EzAppContext::EzAppContext(appinitdata_ptr_t initdata)
    : _initdata(initdata) {
  ork::SetCurrentThreadName("main");
  _stringpoolctx = std::make_shared<StringPoolContext>();
  StringPoolStack::Push(_stringpoolctx);
  /////////////////////////////////////////////
  _mainq  = ork::opq::mainSerialQueue();
  _trackq = new opq::TrackCurrent(_mainq);
  /////////////////////////////////////////////
  for (auto item : initdata->_preinitoperations)
    item();
  /////////////////////////////////////////////
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");
  /////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
EzAppContext::~EzAppContext() {

  if (_initdata->_imgui) {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  StringPoolStack::Pop();
}
///////////////////////////////////////////////////////////////////////////////
orkezapp_ptr_t OrkEzApp::create(appinitdata_ptr_t initdata) {
  //static auto& qti = qtinit(argc, argv, init);
  // QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  lev2::initModule(initdata);
  if (initdata->_imgui) {
    imgui::initModule(initdata);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    Environment env;
    env.init_from_envp(initdata->_envp);
    std::string home_out;
    static file::Path imgui_ini_path;
    if( env.get("HOME", home_out ) ) {
      auto base = file::Path(home_out);
      imgui_ini_path = base / FormatString(".%s-imgui.ini",initdata->_application_name.c_str());
      logchan_ezapp->log( "imgui_ini_path<%s>", imgui_ini_path.c_str() );
    }
    else{
      OrkAssert(false); // HOME not set ???
    }

    io.IniFilename = strdup(imgui_ini_path.c_str());

    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;
    //  Setup Dear ImGui style
    ImGui::StyleColorsDark();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    // ImGuiStyle& style = ImGui::GetStyle();
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    //{
    // style.WindowRounding = 0.0f;
    // style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    //}
  }
  auto ezapp = std::make_shared<OrkEzApp>(initdata);
  if (initdata->_imgui) {
    auto ezwin = ezapp->_mainWindow;
    ImGui_ImplGlfw_InitForOpenGL(ezwin->_ctqt->_glfwWindow, true);
    #if defined(OPENGL_460)
    ImGui_ImplOpenGL3_Init("#version 460 core");
    #elif defined(OPENGL_410)
    ImGui_ImplOpenGL3_Init("#version 410 core");
    #else
    ImGui_ImplOpenGL3_Init("#version 400 core");
    #endif
  }
  return ezapp;
}
///////////////////////////////////////////////////////////////////////////////
orkezapp_ptr_t OrkEzApp::createWithScene(varmap::varmap_ptr_t sceneparams) {
  auto initdata = std::make_shared<AppInitData>();
  initModule(initdata);
  auto rval                           = std::make_shared<OrkEzApp>(initdata);
  rval->_mainWindow->_execsceneparams = sceneparams;
  rval->_mainWindow->_onDraw          = [=](ui::drawevent_constptr_t drwev) { //
    ork::opq::mainSerialQueue()->Process();
    auto context = drwev->GetTarget();
    context->beginFrame();
    rval->_mainWindow->_execscene->renderOnContext(context);
    context->endFrame();
  };
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::enqueueWindowResize(int w, int h){
  if(_mainWindow){
    _mainWindow->enqueueWindowResize(w,h);
  }
}
///////////////////////////////////////////////////////////////////////////////

EzViewport::EzViewport(EzMainWin* mainwin)
    : ui::Viewport("ezviewport", 1, 1, 1, 1, fvec3(0, 0, 0), 1.0f)
    , _mainwin(mainwin) {

  _geometry._w = 1;
  _geometry._h = 1;
  _initstate.store(0);
  lev2::DrawableBuffer::ClearAndSyncWriters();
  _mainwin->_render_timer.Start();
  _mainwin->_render_prevtime = _mainwin->_render_timer.SecsSinceStart();
}
/////////////////////////////////////////////////
void EzViewport::DoInit(ork::lev2::Context* pTARG) {
  pTARG->FBI()->SetClearColor(fcolor4(0.0f, 0.0f, 0.0f, 0.0f));
  FontMan::gpuInit(pTARG);
  _initstate.store(1);
}
/////////////////////////////////////////////////
void EzViewport::DoDraw(ui::drawevent_constptr_t drwev) {

  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();

  //////////////////////////////////////////////////////
  // ensure onUpdateInit called before onGpuInit!
  //////////////////////////////////////////////////////

  auto ezapp = (OrkEzApp*)OrkEzAppBase::get();
  if (not ezapp->checkAppState(KAPPSTATEFLAG_UPDRUNNING))
    return;

  drwev->GetTarget()->makeCurrentContext();

  while (ezapp->_rthreadq->Process()) {
  }

  if (_mainwin->_onDraw) {
    drwev->GetTarget()->makeCurrentContext();
    _mainwin->_onDraw(drwev);
    ezapp->_render_count.fetch_add(1);
  }
  double this_time           = _mainwin->_render_timer.SecsSinceStart();
  double raw_delta           = this_time - _mainwin->_render_prevtime;
  _mainwin->_render_prevtime = this_time;
  _mainwin->_render_stats_timeaccum += raw_delta;
  if (_mainwin->_render_stats_timeaccum >= 5.0) {
    double FPS = _mainwin->_render_state_numiters / _mainwin->_render_stats_timeaccum;
    logchan_ezapp->log("FPS<%g>", FPS);
    _mainwin->_render_stats_timeaccum = 0.0;
    _mainwin->_render_state_numiters  = 0.0;
  } else {
    _mainwin->_render_state_numiters += 1.0;
  }

  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
}
/////////////////////////////////////////////////
void EzViewport::DoSurfaceResize() {
  if (_mainwin->_onResize){
    _mainwin->_onResize(width(), height());
  }
  _topLayoutGroup->SetSize(width(), height());
}
/////////////////////////////////////////////////
ui::HandlerResult EzViewport::DoOnUiEvent(ui::event_constptr_t ev) {
  if (_mainwin->_onUiEvent) {
    auto hacked_event                = std::make_shared<ui::Event>();
    *hacked_event                    = *ev;
    hacked_event->_vpdim.x           = width();
    hacked_event->_vpdim.y           = height();
    return _mainwin->_onUiEvent(hacked_event);
  } else
    return ui::HandlerResult();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool OrkEzApp::checkAppState(uint64_t singlebitmask) const {
  uint64_t chk = _appstate.load() & singlebitmask;
  return chk == singlebitmask;
}
OrkEzAppBase* OrkEzAppBase::_staticapp = nullptr;
OrkEzAppBase* OrkEzAppBase::get() {
  return _staticapp;
}
///////////////////////////////////////////////////////////////////////////////
OrkEzAppBase::OrkEzAppBase(ezappctx_ptr_t ezapp) {
  _staticapp = this;
  _ezapp     = ezapp;
  _update_count = 0;
  _render_count = 0;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::signalExit() {
  _mainWindow->_ctqt->signalExit();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::enqueueOnRenderer(const void_lambda_t& l) {
  _rthreadq->enqueue(l);
}
///////////////////////////////////////////////////////////////////////////////
static std::atomic<OrkEzApp*> __priv_gapp;
void atexit_app(void) { 
  if(__priv_gapp){
    auto app = __priv_gapp.load();
    if(app->_onAppEarlyTerminated){
      app->_onAppEarlyTerminated();
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
OrkEzApp::OrkEzApp(appinitdata_ptr_t initdata)
    : OrkEzAppBase(EzAppContext::get(initdata))
    , _initdata(initdata)
    , _mainWindow(0) 
    , _updateThread("updatethread") {

  __priv_gapp.store(this);
  atexit(atexit_app);

  /////////////////////////////////////////////
  if(initdata->_envp){
    genviron.init_from_envp(initdata->_envp);
    std::string orkdirstr;
    genviron.get("ORKID_WORKSPACE_DIR", orkdirstr);
    _orkidWorkspaceDir = file::Path(orkdirstr);
  }
  /////////////////////////////////////////////
  for (auto op : _initdata->_postinitoperations) {
    op();
  }
  //////////////////////////////////////////////////////////

  _uicontext   = std::make_shared<ui::Context>();
  _update_data = std::make_shared<ui::UpdateData>();
  _appstate    = 0;

  //////////////////////////////////////////////

  _mainWindow = new EzMainWin(*this);

  //////////////////////////////////////
  // create leve gfxwindow
  //////////////////////////////////////
  _mainWindow->_gfxwin = new AppWindow(nullptr);
  GfxEnv::GetRef().RegisterWinContext(_mainWindow->_gfxwin);
  //////////////////////////////////////
  //////////////////////////////////////
  _ezviewport                       = std::make_shared<EzViewport>(_mainWindow);
  _ezviewport->_uicontext           = _uicontext.get();
  _mainWindow->_gfxwin->mRootWidget = _ezviewport.get();
  _ezviewport->_topLayoutGroup = _uicontext->makeTop<ui::LayoutGroup>("top-layoutgroup", 0, 0, _initdata->_width, _initdata->_height);
  _topLayoutGroup              = _ezviewport->_topLayoutGroup;
  /////////////////////////////////////////////
  _updq     = ork::opq::updateSerialQueue();
  _conq     = ork::opq::concurrentQueue();
  _mainq    = ork::opq::mainSerialQueue();
  _rthreadq = std::make_shared<opq::OperationsQueue>(0, "renderSerialQueue");
  /////////////////////////////////////////////
  _mainWindow->_ctqt = new CtxGLFW(_mainWindow->_gfxwin);
  _mainWindow->_ctqt->initWithData(_initdata);
  /////////////////////////////////////////////
  // mainthread runloop callback
  /////////////////////////////////////////////
  _mainWindow->_ctqt->_onRunLoopIteration = [this]() {
    //////////////////////////////
    // handle main serialqueue
    //////////////////////////////
    opq::TrackCurrent opqtest(_mainq);
    _mainq->Process();
    //////////////////////////////
  };

  //////////////////////////////////////////////
  _mainWindow->_ctqt->pushRefreshPolicy(RefreshPolicyItem{EREFRESH_WHENDIRTY});
  /////////////////////////////////////////////
  if (not genviron.has("ORKID_DISABLE_DBLOCK_PROGRESS")) {
    auto handler = [this](opq::progressdata_ptr_t data) { //
      if (_ezviewport->_initstate.load() == 1) {
        _mainWindow->_ctqt->progressHandler(data);
      } else {
      }
    };
    opq::setProgressHandler(handler);
  }

  _mainWindow->_ctqt->Show();
}

///////////////////////////////////////////////////////////////////////////////

OrkEzApp::~OrkEzApp() {
  // printf( "OrkEzApp<%p> destructor - joining update thread...\n", this );
  // printf( "OrkEzApp<%p> destructor - joined update thread\n", this );
  // printf( "OrkEzApp<%p> terminating drawable buffers..\n", this );
  DrawableBuffer::terminateAll();
  __priv_gapp.store(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::joinUpdate() {
  uint64_t prevappsate = _appstate.fetch_or(KAPPSTATEFLAG_JOINED);
  ////////////////////////////////////////////////
  bool has_joined_already = bool(prevappsate & KAPPSTATEFLAG_JOINED);
  ////////////////////////////////////////////////
  if (not has_joined_already) {
    // printf( "OrkEzApp<%p> joinUpdate\n", this );
    while (checkAppState(KAPPSTATEFLAG_UPDRUNNING)) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
    }
    _updq->drain();
    _updateThread.join();
    DrawableBuffer::ClearAndSyncWriters();
  }
  ////////////////////////////////////////////////
}

bool OrkEzApp::isExiting() const {
  return checkAppState(KAPPSTATEFLAG_JOINED);
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::OnTimer() {
  opq::TrackCurrent opqtest(_mainq);
  while (_mainq->Process())
    ;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onDraw(EzMainWin::drawcb_t cb) {
  _mainWindow->_onDraw = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onResize(EzMainWin::onresizecb_t cb) {
  _mainWindow->_onResize = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuInit(EzMainWin::ongpuinit_t cb) {
  _mainWindow->_onGpuInit = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuExit(EzMainWin::ongpuexit_t cb) {
  _mainWindow->_onGpuExit = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUiEvent(EzMainWin::onuieventcb_t cb) {
  _ezviewport->_topLayoutGroup->_evhandler = cb;
  _mainWindow->_onUiEvent                  = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdate(EzMainWin::onupdate_t cb) {
  _mainWindow->_onUpdate = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdateInit(EzMainWin::onupdateinit_t cb) {
  _mainWindow->_onUpdateInit = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdateExit(EzMainWin::onupdateexit_t cb) {
  _mainWindow->_onUpdateExit = cb;
}
/////////////////////////////////////////////////////////////////////////////////
//void OrkEzApp::onUpdateWithScene(EzMainWin::onupdatewithscene_t cb) {
  //_mainWindow->_onUpdateWithScene = cb;
//}
///////////////////////////////////////////////////////////////////////////////
filedevctx_ptr_t OrkEzApp::newFileDevContext(std::string uriproto, const file::Path& basepath) {
  return FileEnv::createContextForUriBase(uriproto, basepath);
}
///////////////////////////////////////////////////////////////////////////////
int OrkEzApp::mainThreadLoop() {

  auto glfw_ctx = _mainWindow->_ctqt;

  ///////////////////////////////
  // update thread implementation
  ///////////////////////////////

  auto update_thread_impl = [&](anyp data) {
    _update_timer.Start();
    _update_prevtime        = _update_timer.SecsSinceStart();
    _update_timeaccumulator = 0.0;
    ork::SetCurrentThreadName("update");
    opq::TrackCurrent opqtest(_updq);
    double stats_timeaccum = 0;
    double state_numiters  = 0.0;

    ////////////////////////////////////////
    // first time init ?
    ////////////////////////////////////////

    if (_mainWindow->_onUpdateInit) {
      _mainWindow->_onUpdateInit();
    }

    _appstate.fetch_or(KAPPSTATEFLAG_UPDRUNNING);

    ////////////////////////////////////////



    while (not checkAppState(KAPPSTATEFLAG_JOINED)) {
      double this_time = _update_timer.SecsSinceStart();
      double raw_delta = this_time - _update_prevtime;
      _update_prevtime = this_time;
      _update_timeaccumulator += raw_delta;
      double step = 1.0 / 240.0;

      while (_update_timeaccumulator >= step) {

        bool do_update = bool(_mainWindow->_onUpdate);

        if (do_update) {
          _update_data->_dt = step;
          _update_data->_abstime += step;
          /////////////////////////////
          /////////////////////////////
          if (not checkAppState(KAPPSTATEFLAG_JOINED)) {
            if (_mainWindow->_onUpdate) {
              _mainWindow->_onUpdate(_update_data);
            } else if (_mainWindow->_onUpdateWithScene) {
              _mainWindow->_onUpdateWithScene(_update_data, _mainWindow->_execscene);
            }
            _update_count.fetch_add(1);
          }
          /////////////////////////////
          /////////////////////////////
          state_numiters += 1.0;
        }

        _update_timeaccumulator -= step;
        stats_timeaccum += step;
        if (stats_timeaccum >= 5.0) {

          logchan_ezapp->log("UPS<%g>", state_numiters / stats_timeaccum);
          stats_timeaccum = 0.0;
          state_numiters  = 0.0;
        }
      }
      opq::updateSerialQueue()->Process();
      ork::usleep(100);
      sched_yield();
    } // while (not checkAppState(KAPPSTATEFLAG_JOINED)) {

    _appstate.fetch_xor(KAPPSTATEFLAG_UPDRUNNING);

    if (_mainWindow->_onUpdateExit) {
      _mainWindow->_onUpdateExit();
    }

  };

  ///////////////////////////////
  // hookup on gpuinit callback
  //   ensuring _onGpuInit called before onUpdateInit
  ///////////////////////////////

  glfw_ctx->_onGpuInit = [this,update_thread_impl](lev2::Context* context){
    
    if(_mainWindow->_onGpuInit){
      _mainWindow->_onGpuInit(context);
    }

    _updateThread.start(update_thread_impl);

  };

  ///////////////////////////////
  // hookup on gpuexit callback
  //   ensuring onGpuExit called after onUpdateExit
  ///////////////////////////////

  glfw_ctx->_onGpuExit = [this](lev2::Context* context){

    joinUpdate();

    if(_mainWindow->_onGpuExit){
      _mainWindow->_onGpuExit(context);
    }
  };

  ///////////////////////////////

  return glfw_ctx->runloop();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::setRefreshPolicy(RefreshPolicyItem policy) {
  _mainWindow->_ctqt->_setRefreshPolicy(policy);
}
///////////////////////////////////////////////////////////////////////////////
EzMainWin::EzMainWin(OrkEzApp& app)
    : _app(app) {
  _execsceneparams = std::make_shared<varmap::VarMap>();
  _update_rendersync = app._initdata->_update_rendersync;
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::enqueueWindowResize(int w, int h){
  _ctqt->enqueueWindowResize(w,h);
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_updateEnqueueLockedAndReleaseFrame(DrawableBuffer* dbuf) {
  //if(_app._initdata->_update_rendersync){
    //DrawableBuffer::releaseFromWriteLocked(dbuf);
  //}
  //else{
    //DrawableBuffer::releaseFromWrite(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_updateEnqueueUnlockedAndReleaseFrame(DrawableBuffer* dbuf) {
  //if(_app._initdata->_update_rendersync){
    //DrawableBuffer::releaseFromWriteLocked(dbuf);
  //}
  //else{
    //DrawableBuffer::releaseFromWrite(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
const DrawableBuffer* EzMainWin::_tryAcquireDrawBuffer(ui::drawevent_constptr_t drawEvent) {
  //_curframecontext = drawEvent->GetTarget();

  //if(_app._initdata->_update_rendersync){
    //return DrawableBuffer::acquireForReadLocked();
  //}
  //else {
    //return DrawableBuffer::acquireForRead(7);
  //
return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
DrawableBuffer* EzMainWin::_tryAcquireUpdateBuffer() {
  DrawableBuffer* rval = nullptr;
  //if(_app._initdata->_update_rendersync){
    //rval = DrawableBuffer::acquireForWriteLocked();
  //}
  //else {
    //rval = DrawableBuffer::acquireForWrite(7);
  //}
  //rval->Reset();
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_releaseAcquireUpdateBuffer(DrawableBuffer*db){
  //if(_app._initdata->_update_rendersync){
//    DrawableBuffer::releaseFromWriteLocked(db);
  //}
  //else {
    //DrawableBuffer::releaseFromWrite(db);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_beginFrame(const DrawableBuffer* dbuf) {
  auto try_ctx = dbuf->getUserProperty("CONTEXT"_crcu);
  _curframecontext->beginFrame();
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_endFrame(const DrawableBuffer* dbuf) {
  if (_update_rendersync) {
    //auto do_rlock = dbuf->getUserProperty("RENDERLOCK"_crcu);
    //if (auto as_lock = do_rlock.tryAs<atom_rlock_ptr_t>()) {
      //as_lock.value()->store(1);
    //}
  }
  _curframecontext->endFrame();
  //if(_app._initdata->_update_rendersync){
    //DrawableBuffer::releaseFromReadLocked(dbuf);
  //}
  //else{
    //DrawableBuffer::releaseFromRead(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::withAcquiredUpdateDrawBuffer(int debugcode, std::function<void(const AcquiredUpdateDrawBuffer&)> l) {
  DrawableBuffer* DB = nullptr;

  if(_update_rendersync){
    //DB = DrawableBuffer::acquireForWriteLocked();
  }
  else{
    //DB = DrawableBuffer::acquireForWrite(debugcode);
  }

  if (DB) {
    DB->Reset();
    AcquiredUpdateDrawBuffer udb;
    udb._DB = DB;
    l(udb);
    if (_update_rendersync)
      _updateEnqueueLockedAndReleaseFrame(DB);
    else
      _updateEnqueueUnlockedAndReleaseFrame(DB);
  }
}
///////////////////////////////////////////////////////////////////////////////
EzMainWin::~EzMainWin() {
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
