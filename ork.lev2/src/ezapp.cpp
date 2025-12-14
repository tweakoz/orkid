#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/vr/vr.h>
#include <boost/program_options.hpp>
#include <ork/kernel/environment.h>
#include <ork/util/logger.h>
#include <ork/lev2/gfx/util/movie.inl>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/stream/audiodevice_stream.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/profiling.inl>
#include <unistd.h>
#if defined(__linux__)
#include <ork/lev2/drm/drm_types.h>
#include <ork/lev2/drm/ctx_drm.h>
#endif

using namespace std::string_literals;

namespace ork {
void initModule(ork::appinitdata_ptr_t init_data);
void exitModule(ork::appinitdata_ptr_t init_data);
} // namespace ork

namespace ork::lev2 {
extern appinitdata_ptr_t _ginitdata;
void initModule(ork::appinitdata_ptr_t init_data);
void exitModule(ork::appinitdata_ptr_t init_data);
} // namespace ork::lev2

namespace ork::lev2 {
extern bool g_allow_HIDPI;
extern context_ptr_t gloadercontext;

static logchannel_ptr_t logchan_ezapp = logger()->configureChannel("EZAPP", fvec3(0.7, 0.7, 0.9), true);

////////////////////////////////////////////////////////////////////////////////
EzUiEventInterceptor::EzUiEventInterceptor()
    : Widget("UiEventInterceptor", 0, 0, 0, 0) {
  _enableDraw = false;
  _vars       = std::make_shared<varmap::VarMap>();
}
ui::HandlerResult EzUiEventInterceptor::DoOnUiEvent(ui::event_constptr_t ev) {
  if (_onUiEventLambda) {
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
  // initModule(initdata);
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
  if(auto e = getenv("ORKID_LOGGER_BACKEND")) {
    std::string backend_str = e;
    if(backend_str.find("HTTP")!=std::string::npos) {
      logchan_ezapp->_perf_interval = 0.1f;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
EzAppContext::~EzAppContext() {

  StringPoolStack::pop();
  ork::lev2::exitModule(_initdata);
  ork::exitModule(_initdata);
}
///////////////////////////////////////////////////////////////////////////////
boost::program_options::options_description_easy_init OrkEzApp::createDefaultOptions( //
  appinitdata_ptr_t init_data, std::string appinfo){ //

  auto desc = init_data->commandLineOptions(appinfo.c_str());

  auto rval = desc->add_options()                                                        //
              ("help", "produce help message")                                           //
              ("msaa", po::value<int>()->default_value(1), "msaa samples(*1,4,9,16,25)") //
              ("ssaa", po::value<int>()->default_value(1), "ssaa samples(*1,4,9,16,25)") //
              ("forward", po::bool_switch()->default_value(false), "forward renderer")   //
              ("fullscreen", po::bool_switch()->default_value(false), "fullscreen mode") //
              ("left", po::value<int>()->default_value(100), "left window offset")       //
              ("top", po::value<int>()->default_value(100), "top window offset")         //
              ("width", po::value<int>()->default_value(1280), "window width")           //
              ("height", po::value<int>()->default_value(720), "window height")          //
              ("usevr", po::bool_switch()->default_value(false), "use vr output")        //
              ("drm", po::value<std::string>(), "DRM mode (e.g., a0, b2, c1) [Linux only]") //
              ("drm-list", po::bool_switch()->default_value(false), "List DRM displays and exit [Linux only]")(
                  "nvmfa", po::value<int>()->default_value(1), "max prerender frames (NVidia)")(
                  "nvsync", po::value<bool>()->default_value(true), "force vsync (NVidia)")(
                  "nvsport", po::value<int>()->default_value(0), "vsync port # (0..3 -> DFP-0..DFP-3) (NVidia)");

  return rval;
}
///////////////////////////////////////////////////////////////////////////////
orkezapp_ptr_t OrkEzApp::create(appinitdata_ptr_t initdata) {
  // static auto& qti = qtinit(argc, argv, init);
  //  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  lev2::initModule(initdata);
  auto ezapp = std::make_shared<OrkEzApp>(initdata);
  return ezapp;
}
///////////////////////////////////////////////////////////////////////////////
orkezapp_ptr_t OrkEzApp::createWithScene(varmap::varmap_ptr_t sceneparams) {
  auto initdata = std::make_shared<AppInitData>();
  // initModule(initdata);
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
void OrkEzApp::enqueueWindowResize(int w, int h) {
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
  finishMovieRecording();
  if (_mainWindow and _mainWindow->_ctqt) {
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

  logchan_ezapp->_status_interval = 5.0f;

  __priv_gapp.store(this);
  /////////////////////////////////////////////
  for (auto op_item : _initdata->_postinitoperations) {
    int order      = op_item.first;
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
  _updq        = ork::opq::updateSerialQueue();
  _conq        = ork::opq::concurrentQueue();
  _mainq       = ork::opq::mainSerialQueue();

  if (_initdata->_enable_graphics) {

#if defined(__linux__)
    // Handle --drm-list option
    if (_initdata->_miscvars.find("drm-list") != _initdata->_miscvars.end()) {
      auto& drm_list_var = _initdata->_miscvars.at("drm-list");
      if (drm_list_var.isA<bool>() && drm_list_var.get<bool>()) {
        drm::DRMContext::listMonitorsAndExit();
      }
    }
#endif

    logchan_ezapp->log("initializing graphics");
    fflush(stdout);
    _appstate = 0;

    _uicontext = std::make_shared<ui::Context>();

    //////////////////////////////////////////////

    _mainWindow = std::make_shared<EzMainWin>(*this);

    //////////////////////////////////////
    // create leve gfxwindow
    //////////////////////////////////////
    _mainWindow->_appwin           = std::make_shared<AppWindow>(nullptr);
    _mainWindow->_appwin->miWidth  = _initdata->_width;
    _mainWindow->_appwin->miHeight = _initdata->_height;
    GfxEnv::GetRef().RegisterWinContext(_mainWindow->_appwin.get());
    //////////////////////////////////////
    //////////////////////////////////////
    _eztopwidget = std::make_shared<EzTopWidget>(_mainWindow.get());
    if (initdata->_disableMouseCursor) {
      _eztopwidget->_clipEvents = false;
    }
    _eztopwidget->_uicontext          = _uicontext.get();
    _mainWindow->_appwin->_rootWidget = _eztopwidget;
    _eztopwidget->_topLayoutGroup =
        _uicontext->makeTop<ui::LayoutGroup>("ezapp-top-layoutgroup", 0, 0, _initdata->_width, _initdata->_height);
    _topLayoutGroup = _eztopwidget->_topLayoutGroup;
    if (initdata->_disableMouseCursor) {
      _topLayoutGroup->_clipEvents = false;
    }

    // Create platform-specific context
#if defined(__linux__)
    if (_initdata->_use_drm) {
      logchan_ezapp->log("Creating DRM context (mode: %s)", _initdata->_drm_mode.c_str());
      _mainWindow->_ctqt = new CtxDRM(_mainWindow->_appwin.get());
    } else
#endif
    {
      _mainWindow->_ctqt = new CtxGLFW(_mainWindow->_appwin.get());
    }
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

      if (this->_onRunLoopIteration) {
        this->_onRunLoopIteration();
      }
      //////////////////////////////
    };
    //////////////////////////////////////////////
    _mainWindow->_ctqt->pushRefreshPolicy(RefreshPolicyItem{EREFRESH_WHENDIRTY});
    _mainWindow->_ctqt->Show();

    /////////////////////////////////////////////
    _rthreadq = std::make_shared<opq::OperationsQueue>(0, "renderSerialQueue");
    /////////////////////////////////////////////
    /////////////////////////////////////////////
    if (not genviron.has("ORKID_DISABLE_DBLOCK_PROGRESS")) {
      auto handler = [this](opq::progressdata_ptr_t data) { //
        if (_eztopwidget->_initstate.load() == 1) {
        } else {
        }
      };
      opq::setProgressHandler(handler);
    }
  } else { // no graphics
    printf("NO GRAPHICS ENABLED\n");
    _mainWindow = nullptr;
    if (_initdata->_enable_audio) {
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
  if (_mainWindow) {
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
    logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:1", this);
    for( int i=0; i<100; i++ ) {
      //checkAppState(KAPPSTATEFLAG_UPDRUNNING)) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
    }
    logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:2", this);
    _updq->drain();
    _updateThread.join();
    logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:3", this);
    DrawQueue::ClearAndSyncWriters();
    logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:4", this);
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
  if (_mainWindow)
    _mainWindow->_onDraw = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onResize(EzMainWin::onresizecallback_t cb) {
  if (_mainWindow)
    _mainWindow->_onResize = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onAudioInit(onauddevfn_t callback) {
  _onAudioInit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onAudioExit(onauddevfn_t callback) {
  _onAudioExit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onSynthInit(onsynfn_t callback) {
  _onSynthInit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onSynthExit(onsynfn_t callback) {
  _onSynthExit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onAppInit(void_lambda_t callback) {
  _onAppInit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onAppExit(void_lambda_t callback) {
  _onAppExit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuInit(EzMainWin::ongpuinit_t cb) {
  if (_mainWindow)
    _mainWindow->_onGpuInit = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuUpdate(EzMainWin::ongpuupdate_t cb) {
  if (_mainWindow)
    _mainWindow->_onGpuUpdate = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuPreFrame(EzMainWin::ongpupreframe_t cb) {
  if (_mainWindow)
    _mainWindow->_onGpuPreFrame = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuPostFrame(EzMainWin::ongpupostframe_t cb) {
  if (_mainWindow) {
    _mainWindow->_onGpuPostFrame = cb;
  }
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onGpuExit(EzMainWin::ongpuexit_t cb) {
  if (_mainWindow) {
    _mainWindow->_onGpuExit = cb;
  }
  _moviecapcontext = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUiEvent(EzMainWin::onuieventcallback_t cb) {
  if (_eztopwidget) {
    _eztopwidget->_topLayoutGroup->_evhandler = cb;
  }
  // OrkBreak();
  if (_mainWindow)
    _mainWindow->_onUiEvent = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdate(EzMainWin::onupdate_t cb) {
  if (_mainWindow) {
    _mainWindow->_onUpdate = cb;
  }
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdateInit(EzMainWin::onupdateinit_t cb) {
  if (_mainWindow)
    _mainWindow->_onUpdateInit = cb;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onUpdateExit(EzMainWin::onupdateexit_t cb) {
  if (_mainWindow)
    _mainWindow->_onUpdateExit = cb;
}
///////////////////////////////////////////////////////////////////////////////
filedevctx_ptr_t OrkEzApp::newFileDevContext(std::string uriproto, const file::Path& basepath) {
  return FileEnv::createContextForUriBase(uriproto, basepath);
}
///////////////////////////////////////////////////////////////////////////////
bool OrkEzApp::shouldUpdateThrottleOnGPU() {
  bool current       = _gpuFrameCounterUP == _gpuFrameCounter;
  _gpuFrameCounterUP = _gpuFrameCounter;
  return not current;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_audioInit() {
  logchan_ezapp->log("OrkEzApp::_audioInit");
  _audiodevice = AudioDevice::createInstance(_initdata);
  _initdata->_miscvars["audiodevice"].set<audiodevice_ptr_t>(_audiodevice);
  if (_initdata->_enable_audio_synth) {
    audio::singularity::synth::bringUp();
    _synth = audio::singularity::synth::instance();
    _initdata->_miscvars["synth"].set<audio::singularity::synth_ptr_t>(_synth);
    if (_synth) {
      _synth->mainThreadHandler();
      if (_onSynthInit) {
        _onSynthInit(_synth);
      }
    }
  }
  if (_onAudioInit) {
    _onAudioInit(_audiodevice);
  }
  _audiodevice->startup();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_audioExit() {
  auto it_a = _initdata->_miscvars.find("audiodevice");
  if (it_a != _initdata->_miscvars.end()) {
    auto auddev = it_a->second.get<audiodevice_ptr_t>();
    if (_audiodevice) {
      _audiodevice->shutdown();
      if (_onAudioExit) {
        _onAudioExit(auddev);
      }
      if (_onSynthExit and _synth) {
        _onSynthExit(_synth);
      }
    }
    _initdata->_miscvars.erase(it_a);
  }
  _audiodevice = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_mainThreadLoopBegin() {
  ///////////////////////////////
  // update thread implementation
  ///////////////////////////////

  _update_thread_impl = [&](anyp data) {
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
    // Determine mode: SYNC or ASYNC (realtime)
    ////////////////////////////////////////

    float target_ups = _initdata->_target_ups;
    float target_fps = _initdata->_target_fps;

    if (_initdata->_freerunning) {

      logchan_ezapp->log("FREERUNNING MODE: realtime, tgt UPS<%g> tgt FPS<%g>", target_ups, target_fps);

      ////////////////////////////////////////
      // FREERUNNING MODE: Wall clock, existing behavior
      ////////////////////////////////////////
      double step = 1.0 / _initdata->_target_ups;
      double max_update_time = 0.0;  // Track max update time in current window (seconds)
      ork::Timer update_timer;

      while (not checkAppState(KAPPSTATEFLAG_JOINING)) {

        EASY_BLOCK("UpdateIteration");
        double this_time = _update_timer.SecsSinceStart() * _timescale;
        double raw_delta = this_time - _update_prevtime;
        _update_prevtime = this_time;
        _update_timeaccumulator += raw_delta;

        if (_update_timeaccumulator >= step) {

          bool do_update = bool(_mainWindow->_onUpdate);

          if (do_update) {
            update_timer.Start();  // Start timing this update
            _update_data->_dt = step;
            _update_data->_abstime += step;
            _update_data->_counter = _update_count.load();
            /////////////////////////////
            /////////////////////////////
            if (not checkAppState(KAPPSTATEFLAG_JOINING)) {
              if (_mainWindow->_onUpdateInternal) {
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
            double update_duration = update_timer.SecsSinceStart();
            if (update_duration > max_update_time) {
              max_update_time = update_duration;
            }
            state_numiters += 1.0;
          }

          _update_timeaccumulator -= step;
          stats_timeaccum += step;
          if (_initdata->_log_freerun_ups && stats_timeaccum >= logchan_ezapp->_perf_interval) {
            logchan_ezapp->perfItem("FREERUN_UPS", float(state_numiters / stats_timeaccum));
            logchan_ezapp->perfItem("FREERUN_MAXU", float(max_update_time * 1000.0));  // Convert to msec
            stats_timeaccum = 0.0;
            state_numiters  = 0.0;
            max_update_time = 0.0;
          }
        }
        opq::updateSerialQueue()->Process();
        sched_yield();
      } // while (not checkAppState(KAPPSTATEFLAG_JOINING)) {
    } // end async mode
    else { // lockstep / sync mode

      logchan_ezapp->log("LockStep/Synchronous MODE: UPS=%g FPS=%g", target_ups, target_fps);

      ////////////////////////////////////////
      // SYNCHRONOUS MODE: Virtual time, deterministic
      ////////////////////////////////////////

      double virtual_time = 0.0;
      double update_delta = 1.0 / target_ups;
      double frame_delta  = 1.0 / target_fps;

      // Get StrAudioDevice if available
      auto str_audio = std::dynamic_pointer_cast<StrAudioDevice>(_audiodevice);

      // Wall-clock tracking for real-time performance
      ork::Timer wallclock_timer;
      wallclock_timer.Start();
      double wallclock_accum = 0.0;
      double wallclock_updates = 0.0;

      while (not checkAppState(KAPPSTATEFLAG_JOINING)) {
        EASY_BLOCK("UpdateIteration_SYNC");

        // Fixed time step per update
        virtual_time += update_delta;
        _render_timeaccumulator += update_delta;

        // Run update
        bool do_update = bool(_mainWindow->_onUpdate);
        if (do_update) {
          _update_data->_dt      = update_delta;
          _update_data->_abstime = virtual_time;
          _update_data->_counter = _update_count.load();
          // printf( "OrkEzApp<%p> update dt<%g> abstime<%g> count<%d>\n", this, _update_data->_dt, _update_data->_abstime, (int)
          // _update_data->_counter );
          /////////////////////////////
          if (not checkAppState(KAPPSTATEFLAG_JOINING)) {
            if (_mainWindow->_onUpdateInternal) {
              _mainWindow->_onUpdateInternal(_update_data);
            }
            if (_mainWindow->_onUpdate) {
              _mainWindow->_onUpdate(_update_data);
            } else if (_mainWindow->_onUpdateWithScene) {
              _mainWindow->_onUpdateWithScene(_update_data, _mainWindow->_execscene);
            }
            _update_count.fetch_add(1);
          }

          state_numiters += 1.0;
          wallclock_updates += 1.0;
        }

        // Log real-time UPS performance
        if (_initdata->_log_lockstep_ups) {
          double elapsed = wallclock_timer.SecsSinceStart();
          if (elapsed >= logchan_ezapp->_status_interval) {
            double real_ups = wallclock_updates / elapsed;
            double pct_of_target = (real_ups / target_ups) * 100.0;
            logchan_ezapp->log("LOCKSTEP_UPS<%g> pct_of_tgtUPS<%g>", real_ups, pct_of_target);
            wallclock_timer.Start();
            wallclock_updates = 0.0;
          }
        }

        // Advance audio by update delta (tied to simulation time)
        if (str_audio && str_audio->_mode == StrAudioDevice::Mode::SYNC_NONREALTIME) {
          auto op = [=](){
            this->_total_samples_rendered = str_audio->advanceTime(update_delta);
          };
          //op();
          opq::auxSerialQueue()->enqueue(op);
        }

        // Check if we should render a frame
        while (_render_timeaccumulator >= frame_delta) {
          _lockstep_frame_requests.fetch_add(1);
          _render_timeaccumulator -= frame_delta;
        }
        while ((not checkAppState(KAPPSTATEFLAG_JOINING)) and (_lockstep_frame_requests.load() > 0)) {
          sched_yield();
        }

        opq::updateSerialQueue()->Process();

      } // while (not checkAppState(KAPPSTATEFLAG_JOINING)) {

    } // end sync mode

    // printf( "update_thread_impl loop exiting\n");

    _appstate.fetch_or(KAPPSTATEFLAG_JOINED);
    _appstate.fetch_and(~KAPPSTATEFLAG_UPDRUNNING);

    if (_mainWindow->_onUpdateExit) {
      // printf( "running _onUpdateExit\n");
      _mainWindow->_onUpdateExit();
    }

    _audioExit();
    // printf( "update_thread exited.....\n");
  };
  EASY_PROFILER_ENABLE;
  //EASY_MAIN_THREAD;
  //profiler::startListen();

  if (not _mainWindow) {
    while (this->_onRunLoopIteration) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
      this->_onRunLoopIteration();
    }
    return;
  }

  auto ctx = _mainWindow->_ctqt;

  this->_gpuFrameCounter++;

  ///////////////////////////////
  // hookup on gpuinit callback
  //   ensuring _onGpuInit called before onUpdateInit
  ///////////////////////////////

  // Enable movie recording BEFORE GPU init if requested
  if (not _initdata->_movie_output_path.empty()) {
    auto settings = std::make_shared<MovieCaptureSettings>();
    settings->_filename = _initdata->_movie_output_path.toAbsolute().toStdString();
    settings->_width    = _initdata->_width;
    settings->_height   = _initdata->_height;
    settings->_fps = _initdata->_target_fps > 0 ? _initdata->_target_fps : 30.0f;
    enableMovieRecording(settings);
  }

  ctx->_onGpuInit = [this](lev2::Context* context) {
    logchan_ezapp->log("BEGIN OrkEzApp::_onGpuInit");
    context->beginPrimaryCommandBuffer();
    logchan_ezapp->log("_initdata->_enable_audio<%d>", (int)_initdata->_enable_audio);

    if (_ginitdata->_disableMouseCursor) {
      auto ctxbase = context->GetCtxBase();
      ctxbase->disableMouseCursor();
    }

    if (_initdata->_enable_audio) {
      _audioInit();
    }

    if (_mainWindow->_onGpuInit) {
      _mainWindow->_onGpuInit(context);
    }
    context->endPrimaryCommandBuffer();

    logchan_ezapp->log("END OrkEzApp::_onGpuInit");
    logchan_ezapp->log("starting update thread...");
    _updateThread.start(_update_thread_impl);
    // Note: gpuPostInit() will be called by the framework (CtxGLFW::_runloopBegin)
  };

  ///////////////////////////////

  ctx->_onGpuUpdate = [this](lev2::Context* context) {
    this->_gpuFrameCounter++;

    if (_mainWindow->_onGpuUpdate) {
      _mainWindow->_onGpuUpdate(context);
    }
  };
  /*
  ctx->_onGpuPreFrame = [this](lev2::Context* context) {
    if (_mainWindow->_onGpuPreFrame) {
      _mainWindow->_onGpuPreFrame(context);
    }
  };
  ctx->_onGpuPostFrame = [this](lev2::Context* context) {
    if (_mainWindow->_onGpuPostFrame) {
      _mainWindow->_onGpuPostFrame(context);
    }
    if( _movie_record_frame_lambda ) {
      _movie_record_frame_lambda( context );
    }
  };*/

  ///////////////////////////////
  // hookup on gpuexit callback
  //   ensuring onGpuExit called after onUpdateExit
  ///////////////////////////////

  ctx->_onGpuExit = [this](lev2::Context* context) {
    joinUpdate();
    if (_moviecapcontext) {
      _moviecapcontext->terminate();
    }

    if (_mainWindow->_onGpuExit) {
      _mainWindow->_onGpuExit(context);
    }
  };
  ctx->_runloopBegin();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_mainThreadLoopIter() {
  if (_mainWindow) {
    auto ctx = _mainWindow->_ctqt;
    ctx->_runloopIter();
  } else {
    gloadercontext->beginFrame(false);
    gloadercontext->endFrame();
  }
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_mainThreadLoopEnd() {
  if (_mainWindow) {
    auto ctx = _mainWindow->_ctqt;
    ctx->_runloopEnd();
  }
  size_t num_prof_blocks = profiler::dumpBlocksToFile("test_profile.prof");
  logchan_ezapp->log( "Dumped %zu profiler blocks to test_profile.prof\n", num_prof_blocks);
}
///////////////////////////////////////////////////////////////////////////////
int OrkEzApp::mainThreadLoop() {
  _mainThreadLoopBegin();

  // Wall-clock FPS tracking
  ork::Timer fps_timer;
  fps_timer.Start();
  double frame_count = 0.0;
  double max_frame_time = 0.0;  // Track max frame time in current window (seconds)
  ork::Timer frame_timer;

  if (_mainWindow) {
    auto ctx = _mainWindow->_ctqt;
    if (_initdata->_freerunning) {
      while (ctx->_runstate == 1) {
        frame_timer.Start();  // Start timing this frame
        ctx->_runloopIter(true);
        double frame_duration = frame_timer.SecsSinceStart();
        if (frame_duration > max_frame_time) {
          max_frame_time = frame_duration;
        }

        // Track freerun FPS
        if (_initdata->_log_freerun_fps) {
          frame_count += 1.0;
          double elapsed = fps_timer.SecsSinceStart();
          if (elapsed >= logchan_ezapp->_perf_interval) {
            double real_fps = frame_count / elapsed;
            logchan_ezapp->perfItem("FREERUN_FPS", float(real_fps));
            logchan_ezapp->perfItem("FREERUN_MAXF", float(max_frame_time * 1000.0));  // Convert to msec
            frame_count = 0.0;
            max_frame_time = 0.0;
            fps_timer.Start();
          }
        }
      }
    } else {
      while (ctx->_runstate == 1) {
        while (_lockstep_frame_requests.load()) {
          ctx->_runloopIter(false);
          _lockstep_frame_requests.fetch_sub(1);

          // Track lockstep FPS
          if (_initdata->_log_lockstep_fps) {
            frame_count += 1.0;
            double elapsed = fps_timer.SecsSinceStart();
            if (elapsed  >= logchan_ezapp->_status_interval) {
              double real_fps = frame_count / elapsed;
              double pct_of_target = (real_fps / _initdata->_target_fps) * 100.0;
              logchan_ezapp->log("LOCKSTEP_FPS<%g> pct_of_tgtFPS<%g>", real_fps, pct_of_target);
              frame_count = 0.0;
              fps_timer.Start();
            }
          }
        }
        sched_yield();
      }
    }
  }
  _mainThreadLoopEnd();
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::enableMovieRecording(moviecapsettings_ptr_t settings) {
  logchan_ezapp->log("Enabling movie recording to output path<%s> _audiodevice<%p>\n", settings->_filename.c_str(), (void*) _audiodevice.get());
  settings->_audiodevice      = _audiodevice;
  settings->_width            = _initdata->_width;
  settings->_height           = _initdata->_height;
  _moviecapcontext            = std::make_shared<MovieCaptureContext>(settings);
  _moviecapcontext->init();

  auto mctx                  = _moviecapcontext.get();
  _movie_record_frame_lambda = [=](lev2::Context* ctx) {
    auto fbi    = ctx->FBI();
    auto rtg = fbi->_main_rtg;
    auto rtb = rtg->buffer(0);
    if( settings->_rtbuffer ) {
      rtb = settings->_rtbuffer;
    }

    auto capbuf = std::make_shared<CaptureBuffer>();

    captureasync_ptr_t future;
    future = fbi->captureAsFormat( rtb.get(), capbuf, EBufferFormat::RGBA8);

    bool fps_set = (_initdata->_target_fps > 0);
    // Calculate expected audio samples for this frame
    int expected_samples = fps_set                               //
                         ? int(48000.0 / double(_initdata->_target_fps)) //
                         : int(800);

    // Queue for encoding thread (don't wait!)
    int frame_num = _render_count.load();
    size_t enqueued = mctx->enqueueFrame(future, capbuf, frame_num, expected_samples);
  };
}
void OrkEzApp::finishMovieRecording() {
  if(_moviecapcontext){
    _moviecapcontext->terminate();
  }
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::setRefreshPolicy(RefreshPolicyItem policy) {
  if (_mainWindow)
    _mainWindow->_ctqt->_setRefreshPolicy(policy);
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

ork::lev2::orkezapp_ptr_t lev2appinit(ork::appinitdata_ptr_t init_data) {
  ork::SetCurrentThreadName("main");

  ork::genviron.init_from_global_env();

  static auto _init_data = init_data;
  if (_init_data == nullptr) {
    _init_data = std::make_shared<ork::AppInitData>();
  }

  _init_data->_offscreen = true;
  auto ezapp             = ork::lev2::OrkEzApp::create(_init_data);

  ork::lev2::initModule(init_data);

  static std::shared_ptr<ork::lev2::ThreadGfxContext> _gthreadgfxctx;
  _gthreadgfxctx = std::make_shared<ork::lev2::ThreadGfxContext>(ork::lev2::gloadercontext.get());

  ork::lev2::gloadercontext->makeCurrentContext();

  return ezapp;
}
