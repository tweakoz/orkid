#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/aud/singularity/synth.h>
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
#include <ork/lev2/gfx/util/movie.inl>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/profiling.inl>

using namespace std::string_literals;

namespace ork {
  void initModule(ork::appinitdata_ptr_t init_data);
  void exitModule(ork::appinitdata_ptr_t init_data);
}

namespace ork::lev2{
  extern appinitdata_ptr_t _ginitdata;
  void initModule(ork::appinitdata_ptr_t init_data);
  void exitModule(ork::appinitdata_ptr_t init_data);
}

namespace ork::lev2 {
extern bool g_allow_HIDPI;

static logchannel_ptr_t logchan_ezapp = logger()->configureChannel("EZAPP", fvec3(0.7, 0.7, 0.9));

////////////////////////////////////////////////////////////////////////////////
EzUiEventInterceptor::EzUiEventInterceptor()
  : Widget("UiEventInterceptor",0,0,0,0){
  _enableDraw = false;
  _vars = std::make_shared<varmap::VarMap>();
}
ui::HandlerResult EzUiEventInterceptor::DoOnUiEvent(ui::event_constptr_t ev) {
  if(_onUiEventLambda){
    return _onUiEventLambda(ev);
  }
  return ui::HandlerResult();
}
ui::Widget* EzUiEventInterceptor::doRouteUiEvent(ui::event_constptr_t ev) {
  return this;
}
////////////////////////////////////////////////////////////////////////////////
ezappctx_ptr_t EzAppContext::get(appinitdata_ptr_t initdata) {
  if (nullptr == initdata) {
    initdata = std::make_shared<AppInitData>();
  }
  //initModule(initdata);
  static auto app = std::shared_ptr<EzAppContext>(new EzAppContext(initdata));
  return app;
}
////////////////////////////////////////////////////////////////////////////////
EzAppContext::EzAppContext(appinitdata_ptr_t initdata)
    : _initdata(initdata) {
  ork::SetCurrentThreadName("main");
  _stringpoolctx = std::make_shared<StringPoolContext>();
  StringPoolStack::push(_stringpoolctx);
  /////////////////////////////////////////////
  _mainq  = ork::opq::mainSerialQueue();
  _trackq = new opq::TrackCurrent(_mainq);
  /////////////////////////////////////////////
  ork::lev2::initModule(initdata);
  initdata->finalizeInitialization();
  /////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
EzAppContext::~EzAppContext() {

  if (_initdata->_imgui) {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  StringPoolStack::pop();
  ork::lev2::exitModule(_initdata);
  ork::exitModule(_initdata);
}
///////////////////////////////////////////////////////////////////////////////
boost::program_options::options_description_easy_init OrkEzApp::createDefaultOptions( //
  appinitdata_ptr_t init_data, std::string appinfo){ //

  auto desc = init_data->commandLineOptions(appinfo.c_str());

  auto rval = desc->add_options() //
      ("help", "produce help message") //
      ("msaa", po::value<int>()->default_value(1), "msaa samples(*1,4,9,16,25)") //
      ("ssaa", po::value<int>()->default_value(1), "ssaa samples(*1,4,9,16,25)") //
      ("forward", po::bool_switch()->default_value(false), "forward renderer") //
      ("fullscreen", po::bool_switch()->default_value(false), "fullscreen mode") //
      ("left", po::value<int>()->default_value(100), "left window offset") // 
      ("top", po::value<int>()->default_value(100), "top window offset") //
      ("width", po::value<int>()->default_value(1280), "window width") //
      ("height", po::value<int>()->default_value(720), "window height")//
      ("usevr", po::bool_switch()->default_value(false), "use vr output")
      ("nvmfa", po::value<int>()->default_value(1), "max prerender frames (NVidia)")
      ("nvsync", po::value<bool>()->default_value(true), "force vsync (NVidia)")
      ("nvsport", po::value<int>()->default_value(0), "vsync port # (0..3 -> DFP-0..DFP-3) (NVidia)");

  return rval;
}
///////////////////////////////////////////////////////////////////////////////
orkezapp_ptr_t OrkEzApp::create(appinitdata_ptr_t initdata) {
  // static auto& qti = qtinit(argc, argv, init);
  //  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  lev2::initModule(initdata);
  if (initdata->_imgui) {
    lev2::editor::imgui::initModule(initdata);
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    Environment env;
    std::string home_out;
    static file::Path imgui_ini_path;
    if (env.get("OBT_STAGE", home_out)) {
      auto base      = file::Path(home_out);
      imgui_ini_path = base / FormatString(".%s-imgui.ini", initdata->_application_name.c_str());
      logchan_ezapp->log("imgui_ini_path<%s>", imgui_ini_path.c_str());
    } else {
      OrkAssert(false); // HOME not set ???
    }
    if( not imgui_ini_path.doesPathExist() ){
      file::Path try_this_path = file::Path(initdata->_default_imgui_path);
      if( try_this_path.isFile() ){
        // copy default imgui ini file
        auto src = try_this_path.toAbsolute();
        auto dst = imgui_ini_path.toAbsolute();
        std::string cmd_str = FormatString("cp %s %s", src.c_str(), dst.c_str());
        logchan_ezapp->log( "copying default imgui ini file <%s> to <%s>", src.c_str(), dst.c_str());
        int ret = system(cmd_str.c_str());
        OrkAssert(ret == 0);
        OrkAssert(dst.doesPathExist());
      }      
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
  //initModule(initdata);
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
  if (_mainWindow) {
    _mainWindow->enqueueWindowResize(w, h);
  }
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
  _staticapp    = this;
  _ezapp        = ezapp;
  _update_count = 0;
  _render_count = 0;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::signalExit() {
  _onRunLoopIteration = nullptr;
  if( _mainWindow and _mainWindow->_ctqt){
    _mainWindow->_ctqt->signalExit();
  }
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::enqueueOnRenderer(const void_lambda_t& l) {
  _rthreadq->enqueue(l);
}
///////////////////////////////////////////////////////////////////////////////
static std::atomic<OrkEzApp*> __priv_gapp;
void atexit_app(void) {
  if (__priv_gapp) {
    auto app = __priv_gapp.load();
    if (app->_onAppEarlyTerminated) {
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
  /////////////////////////////////////////////
  for (auto op_item : _initdata->_postinitoperations) {
    int order = op_item.first;
    auto operation = op_item.second;
    operation();
  }
  /////////////////////////////////////////////
  atexit(atexit_app);
  _vars = std::make_shared<varmap::VarMap>();
  /////////////////////////////////////////////
  std::string orkdirstr;
  genviron.get("ORKID_WORKSPACE_DIR", orkdirstr);
  _orkidWorkspaceDir = file::Path(orkdirstr);
  //////////////////////////////////////////////////////////

  _update_data = std::make_shared<ui::UpdateData>();
  _updq     = ork::opq::updateSerialQueue();
  _conq     = ork::opq::concurrentQueue();
  _mainq    = ork::opq::mainSerialQueue();

  if(_initdata->_enable_graphics){

    _uicontext   = std::make_shared<ui::Context>();
    _appstate    = 0;

  //////////////////////////////////////////////

    _mainWindow = std::make_shared<EzMainWin>(*this);

    //////////////////////////////////////
    // create leve gfxwindow
    //////////////////////////////////////
    _mainWindow->_appwin = std::make_shared<AppWindow>(nullptr);
    _mainWindow->_appwin->miWidth = _initdata->_width;
    _mainWindow->_appwin->miHeight = _initdata->_height;
    GfxEnv::GetRef().RegisterWinContext(_mainWindow->_appwin.get());
    //////////////////////////////////////
    //////////////////////////////////////
    _eztopwidget                       = std::make_shared<EzTopWidget>(_mainWindow.get());
    if(initdata->_disableMouseCursor){
      _eztopwidget->_clipEvents = false;
    }
    _eztopwidget->_uicontext           = _uicontext.get();
    _mainWindow->_appwin->_rootWidget = _eztopwidget;
    _eztopwidget->_topLayoutGroup =
        _uicontext->makeTop<ui::LayoutGroup>("ezapp-top-layoutgroup", 0, 0, _initdata->_width, _initdata->_height);
    _topLayoutGroup = _eztopwidget->_topLayoutGroup;
    if(initdata->_disableMouseCursor){
      _topLayoutGroup->_clipEvents = false;
    }
    /////////////////////////////////////////////
    _rthreadq = std::make_shared<opq::OperationsQueue>(0, "renderSerialQueue");
    /////////////////////////////////////////////
    _mainWindow->_ctqt = new CtxGLFW(_mainWindow->_appwin.get());
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

      if(this->_onRunLoopIteration){
        this->_onRunLoopIteration();
      }
      //////////////////////////////
    };
    //////////////////////////////////////////////
    _mainWindow->_ctqt->pushRefreshPolicy(RefreshPolicyItem{EREFRESH_WHENDIRTY});
    /////////////////////////////////////////////
    if (not genviron.has("ORKID_DISABLE_DBLOCK_PROGRESS")) {
      auto handler = [this](opq::progressdata_ptr_t data) { //
        if (_eztopwidget->_initstate.load() == 1) {
        } else {
        }
      };
      opq::setProgressHandler(handler);
    }

    _mainWindow->_ctqt->Show();
  }
  else { // no graphics
    _mainWindow = nullptr;
    if(_initdata->_enable_audio){
      logchan_ezapp->log("initializing audio");
      _audioInit();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

OrkEzApp::~OrkEzApp() {
  // printf( "OrkEzApp<%p> destructor - joining update thread...\n", this );
  // printf( "OrkEzApp<%p> destructor - joined update thread\n", this );
  // printf( "OrkEzApp<%p> terminating drawable buffers..\n", this );
  if(_mainWindow){
    DrawQueue::terminateAll();
  }
  __priv_gapp.store(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::joinUpdate() {
  uint64_t prevappsate = _appstate.fetch_or(KAPPSTATEFLAG_JOINING);
  ////////////////////////////////////////////////
  bool has_joined_already = bool(prevappsate & KAPPSTATEFLAG_JOINING);
  ////////////////////////////////////////////////
  if (not has_joined_already) {
     logger()->defaultChannel()->log( "OrkEzApp<%p> joinUpdate:1", this );
    while (checkAppState(KAPPSTATEFLAG_UPDRUNNING)) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
    }
     logger()->defaultChannel()->log( "OrkEzApp<%p> joinUpdate:2", this );
    _updq->drain();
    _updateThread.join();
     logger()->defaultChannel()->log( "OrkEzApp<%p> joinUpdate:3", this );
    DrawQueue::ClearAndSyncWriters();
     logger()->defaultChannel()->log( "OrkEzApp<%p> joinUpdate:4", this );

  }
  ////////////////////////////////////////////////
}

bool OrkEzApp::isExiting() const {
  return checkAppState(KAPPSTATEFLAG_JOINING);
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::OnTimer() {
  opq::TrackCurrent opqtest(_mainq);
  while (_mainq->Process())
    ;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onDraw(EzMainWin::drawcallback_t cb) {
  if(_mainWindow)
    _mainWindow->_onDraw = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onResize(EzMainWin::onresizecallback_t cb) {
  if(_mainWindow)
    _mainWindow->_onResize = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onAudioInit(onauddevfn_t callback){
  _onAudioInit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onAudioExit(onauddevfn_t callback){
  _onAudioExit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onSynthInit(onsynfn_t callback){
    _onSynthInit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onSynthExit(onsynfn_t callback){
  _onSynthExit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onAppExit(void_lambda_t callback){
    _onAppExit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuInit(EzMainWin::ongpuinit_t cb) {
  if(_mainWindow)
    _mainWindow->_onGpuInit = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuUpdate(EzMainWin::ongpuupdate_t cb) {
  if(_mainWindow)
    _mainWindow->_onGpuUpdate = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuPreFrame(EzMainWin::ongpupreframe_t cb) {
  if(_mainWindow)
    _mainWindow->_onGpuPreFrame = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuPostFrame(EzMainWin::ongpupostframe_t cb) {
  if(_mainWindow)
    _mainWindow->_onGpuPostFrame = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuExit(EzMainWin::ongpuexit_t cb) {
  if(_mainWindow){
    _mainWindow->_onGpuExit = cb;
  }
  _moviecontext = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUiEvent(EzMainWin::onuieventcallback_t cb) {
  if(_eztopwidget){
    _eztopwidget->_topLayoutGroup->_evhandler = cb;
  }
  //OrkBreak();
  if(_mainWindow)
    _mainWindow->_onUiEvent                  = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdate(EzMainWin::onupdate_t cb) {
  if(_mainWindow)
    _mainWindow->_onUpdate = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdateInit(EzMainWin::onupdateinit_t cb) {
  if(_mainWindow)
    _mainWindow->_onUpdateInit = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdateExit(EzMainWin::onupdateexit_t cb) {
  if(_mainWindow)
    _mainWindow->_onUpdateExit = cb;
}
///////////////////////////////////////////////////////////////////////////////
filedevctx_ptr_t OrkEzApp::newFileDevContext(std::string uriproto, const file::Path& basepath) {
  return FileEnv::createContextForUriBase(uriproto, basepath);
}
///////////////////////////////////////////////////////////////////////////////
bool OrkEzApp::shouldUpdateThrottleOnGPU(){
    bool current = _gpuFrameCounterUP == _gpuFrameCounter;
    _gpuFrameCounterUP = _gpuFrameCounter;
    return not current;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_audioInit(){
  logchan_ezapp->log("OrkEzApp::_audioInit");
  _audiodevice = AudioDevice::createInstance(_initdata);
  _initdata->_miscvars["audiodevice"].set<audiodevice_ptr_t>(_audiodevice);
  if(_initdata->_enable_audio_synth){
    audio::singularity::synth::bringUp();
    _synth = audio::singularity::synth::instance();
    _initdata->_miscvars["synth"].set<audio::singularity::synth_ptr_t>(_synth);
    if(_synth){
      _synth->mainThreadHandler();
    }
    if(_onSynthInit){
      _onSynthInit(_synth);
    }
  }
  if(_onAudioInit){
    _onAudioInit(_audiodevice);
  }
  _audiodevice->startup();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_audioExit(){
  auto it_a = _initdata->_miscvars.find("audiodevice");
  if(it_a != _initdata->_miscvars.end()){
    auto auddev = it_a->second.get<audiodevice_ptr_t>();
    if(_audiodevice){
      _audiodevice->shutdown();
      if(_onAudioExit){
        _onAudioExit(auddev);
      }
    }
    _initdata->_miscvars.erase(it_a);
  }
  _audiodevice = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
int OrkEzApp::mainThreadLoop() {

  EASY_PROFILER_ENABLE;
  EASY_MAIN_THREAD;
  profiler::startListen();

  if(not _mainWindow){
    while(this->_onRunLoopIteration){
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
      this->_onRunLoopIteration();
    }
    return 0;
  }

  auto glfw_ctx = _mainWindow->_ctqt;

  this->_gpuFrameCounter++;
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

    while (not checkAppState(KAPPSTATEFLAG_JOINING)) {

      EASY_BLOCK("UpdateIteration" );
      double this_time = _update_timer.SecsSinceStart()*_timescale;
      double raw_delta = this_time - _update_prevtime;
      _update_prevtime = this_time;
      _update_timeaccumulator += raw_delta;
      double step = 1.0 / 400.0;

      if(_update_timeaccumulator >= step) {

        bool do_update = bool(_mainWindow->_onUpdate);

        if (do_update) {
          _update_data->_dt = step;
          _update_data->_abstime += step;
          _update_data->_counter = _update_count.load();
          /////////////////////////////
          /////////////////////////////
          if (not checkAppState(KAPPSTATEFLAG_JOINING)) {
            if(_mainWindow->_onUpdateInternal){
              _mainWindow->_onUpdateInternal(_update_data);
            }
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
        if (stats_timeaccum >= logchan_ezapp->_status_interval) {
          logchan_ezapp->status("UPS", "<%g>", state_numiters / stats_timeaccum);
          stats_timeaccum = 0.0;
          state_numiters  = 0.0;
        }
      }
      opq::updateSerialQueue()->Process();
      ork::usleep(100);
      sched_yield();
    } // while (not checkAppState(KAPPSTATEFLAG_JOINING)) {

    //printf( "update_thread_impl loop exiting\n");

    _appstate.fetch_or(KAPPSTATEFLAG_JOINED);
    _appstate.fetch_and(~KAPPSTATEFLAG_UPDRUNNING);

    if (_mainWindow->_onUpdateExit) {
      //printf( "running _onUpdateExit\n");
      _mainWindow->_onUpdateExit();
    }

    _audioExit();
    //printf( "update_thread exited.....\n");
  };

  ///////////////////////////////
  // hookup on gpuinit callback
  //   ensuring _onGpuInit called before onUpdateInit
  ///////////////////////////////

  glfw_ctx->_onGpuInit = [this, update_thread_impl](lev2::Context* context) {

    logchan_ezapp->log("_initdata->_enable_audio<%d>", (int) _initdata->_enable_audio);

    if( _ginitdata->_disableMouseCursor ){
      auto ctxbase = context->GetCtxBase();
      ctxbase->disableMouseCursor();
    }

    if(_initdata->_enable_audio){
      _audioInit();
    }

    if (_mainWindow->_onGpuInit) {
      _mainWindow->_onGpuInit(context);

      if( _moviecontext ){
        _moviecontext->init(_initdata->_width,_initdata->_height);
      }

    }

    _updateThread.start(update_thread_impl);
  };

  ///////////////////////////////

  glfw_ctx->_onGpuUpdate = [this](lev2::Context* context) {
    this->_gpuFrameCounter++;

    if (_mainWindow->_onGpuUpdate) {
      _mainWindow->_onGpuUpdate(context);
    }
  };
  glfw_ctx->_onGpuPreFrame = [this](lev2::Context* context) {
    if (_mainWindow->_onGpuPreFrame) {
      _mainWindow->_onGpuPreFrame(context);
    }
  };
  glfw_ctx->_onGpuPostFrame = [this](lev2::Context* context) {
    if (_mainWindow->_onGpuPostFrame) {
      _mainWindow->_onGpuPostFrame(context);
    }
  };

  ///////////////////////////////
  // hookup on gpuexit callback
  //   ensuring onGpuExit called after onUpdateExit
  ///////////////////////////////

  glfw_ctx->_onGpuExit = [this](lev2::Context* context) {
    joinUpdate();
    if( _moviecontext ){
      _moviecontext->terminate();
    }

    if (_mainWindow->_onGpuExit) {
      _mainWindow->_onGpuExit(context);
    }
  };

  ///////////////////////////////

  return glfw_ctx->runloop();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::enableMovieRecording(file::Path output_path){
    _moviecontext = std::make_shared<MovieContext>();
    _moviecontext->_filename = output_path.toAbsolute().c_str();

    auto mctx = _moviecontext.get();
    _movie_record_frame_lambda = [mctx,this](lev2::Context* ctx){
        auto fbi = ctx->FBI();
        CaptureBuffer capbuf;
        bool ok = fbi->captureAsFormat(nullptr, &capbuf, EBufferFormat::RGB8);
        mctx->writeFrame(capbuf);
        //int ircount = _render_count.load();
        //int iucount = _update_count.load();
        //printf( "movie write frame<%d> ircount<%d> iucount<%d>\n", mctx->_frame, ircount, iucount );
      };
}
void OrkEzApp::finishMovieRecording(){
  _moviecontext->terminate();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::setRefreshPolicy(RefreshPolicyItem policy) {
  if(_mainWindow)
    _mainWindow->_ctqt->_setRefreshPolicy(policy);
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
