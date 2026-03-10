#include <ork/lev2/ezapp.h>
#include <ork/lev2/init.h>
#include <ork/lev2/subsystem_gpu.h>
#include <ork/lev2/subsystem_audio.h>
#include <ork/lev2/subsystem_lev2.h>
#include <ork/application/subsystem_opq.h>
#include <ork/application/subsystem_catalog.h>
#include <ork/application/subsystem_core.h>
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
#if defined(ENABLE_GLFW)
#include <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3.h>
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
  _enable = false;
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
// OrkEzAppBase constructor - now calls Application's derived class constructor
// This enables HFSM lifecycle support while preserving all existing behavior
///////////////////////////////////////////////////////////////////////////////
OrkEzAppBase::OrkEzAppBase(ezappctx_ptr_t ezapp, appinitdata_ptr_t initdata)
    : Application(initdata, true) {  // Call derived-class constructor
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
    : OrkEzAppBase(EzAppContext::get(initdata), initdata)  // Pass initdata to OrkEzAppBase
    , _mainWindow(0)
    , _updateThread("updatethread") {

  logchan_ezapp->_status_interval = 8.0f;

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

  _update_data  = std::make_shared<ui::UpdateData>();
  _update_queue = ork::opq::updateSerialQueue();
  _conq         = ork::opq::concurrentQueue();
  _mainq        = ork::opq::mainSerialQueue();

  /////////////////////////////////////////////
  // Fork initialization path based on use_subsystems flag
  /////////////////////////////////////////////
  if (_initdata->_use_subsystems) {
    _initForSubsystems();
  } else {
    _initForAdHoc();
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
// Legacy ad-hoc initialization (use_subsystems=false)
// Graphics and audio are initialized inline in constructor
///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::_initForAdHoc() {
  logchan_ezapp->log("initForAdHoc - legacy inline initialization");

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
    // create lev2 gfxwindow
    //////////////////////////////////////
    _mainWindow->_appwin           = std::make_shared<AppWindow>(nullptr);
    _mainWindow->_appwin->miWidth  = _initdata->_width;
    _mainWindow->_appwin->miHeight = _initdata->_height;
    GfxEnv::GetRef().RegisterWinContext(_mainWindow->_appwin.get());
    //////////////////////////////////////
    //////////////////////////////////////
    _eztopwidget = std::make_shared<EzTopWidget>(_mainWindow.get());
    if (_initdata->_disableMouseCursor) {
      _eztopwidget->_clipEvents = false;
    }
    _eztopwidget->_uicontext          = _uicontext.get();
    _mainWindow->_appwin->_rootWidget = _eztopwidget;
    _eztopwidget->_topLayoutGroup =
        _uicontext->makeTop<ui::LayoutGroup>("ezapp-top-layoutgroup", 0, 0, _initdata->_width, _initdata->_height);
    _topLayoutGroup = _eztopwidget->_topLayoutGroup;
    if (_initdata->_disableMouseCursor) {
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
  }
  if (_initdata->_enable_audio) {
    logchan_ezapp->log("initializing audio");
    _audioInit();
  }
}

///////////////////////////////////////////////////////////////////////////////
// HFSM subsystem-driven initialization (use_subsystems=true)
// Graphics and audio init are triggered by subsystem state transitions
///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::_initForSubsystems() {
  logchan_ezapp->log("initForSubsystems - HFSM-driven initialization");

  auto& enabled = _initdata->_enabled_subsystems;
  auto& custom_subsystems = _initdata->_custom_subsystems;
  bool list_mode = !enabled.empty();

  /////////////////////////////////////////////
  // Helper to check if a subsystem is wanted
  /////////////////////////////////////////////

  auto want = [&](const std::string& name) -> bool {
    if (list_mode) {
      return enabled.count(name) > 0;
    }
    // Legacy mode - use existing boolean flags
    if (name == "gpu") return _initdata->_enable_graphics;
    if (name == "audio" || name == "audioI" || name == "audioO" || name == "audioIO") {
      return _initdata->_enable_audio;
    }
    if (name == "catalog") return _initdata->_std_asset_catalog;
    return (name == "opq" || name == "core");
  };

  /////////////////////////////////////////////
  // Handle audio aliases - set flags based on selection
  /////////////////////////////////////////////

  if (list_mode) {
    bool want_audioI  = enabled.count("audioI") > 0;
    bool want_audioO  = enabled.count("audioO") > 0;
    bool want_audioIO = enabled.count("audioIO") > 0;

    if (want_audioI || want_audioO || want_audioIO) {
      _initdata->_enable_audio = true;
      _initdata->_enable_audio_input  = want_audioI || want_audioIO;
      _initdata->_enable_audio_output = want_audioO || want_audioIO;
      _initdata->_enable_audio_synth  = want_audioO || want_audioIO;
    }

    if (enabled.count("gpu") > 0) {
      _initdata->_enable_graphics = true;
    }

    if (enabled.count("catalog") > 0) {
      _initdata->_std_asset_catalog = true;
    } else {
      _initdata->_std_asset_catalog = false;
    }
  }

  /////////////////////////////////////////////
  // Build subsystem map - all subsystems keyed by name
  /////////////////////////////////////////////

  std::map<std::string, subsystem_ptr_t> subsystem_map;

  // Always create opq and core
  auto opq_subsystem = createOpqSubsystem();
  subsystem_map["opq"] = opq_subsystem;

  auto core_subsystem = createCoreSubsystem();
  core_subsystem->_pending_children.push_back("opq");
  subsystem_map["core"] = core_subsystem;

  // Catalog (optional)
  if (want("catalog")) {
    auto catalog_subsystem = createCatalogSubsystem();
    catalog_subsystem->_pending_dependencies.push_back("opq");
    core_subsystem->_pending_children.push_back("catalog");
    subsystem_map["catalog"] = catalog_subsystem;
  }

  // GPU (optional)
  if (want("gpu")) {
    _gpu_subsystem = createGpuSubsystem();
    _gpu_subsystem->_pending_dependencies.push_back("core");
    _gpu_subsystem->_requires_thread = "main";  // GPU/GLFW requires main thread

    // Wire up GPU callbacks
    auto gpu_impl = getGpuSubsystemImpl(_gpu_subsystem);
    gpu_impl->_onGpuInit = [this]() {
      logchan_ezapp->log("GPU subsystem triggering graphics init");
      auto loader_ctx = ensureLoaderContext();
      logchan_ezapp->log("GPU subsystem loader context: %p", (void*)loader_ctx.get());
      _initGraphicsContext();
    };
    gpu_impl->_onGpuExit = [this]() {
      logchan_ezapp->log("GPU subsystem triggering graphics cleanup");
    };

    subsystem_map["gpu"] = _gpu_subsystem;
  }

  // Audio (optional)
  // Audio is a child of lev2, depends on gpu (sibling dependency)
  if (_initdata->_enable_audio) {
    _audio_subsystem = createAudioSubsystem();
    if (want("gpu")) {
      _audio_subsystem->_pending_dependencies.push_back("gpu");
    }

    // Wire up Audio callbacks
    auto audio_impl = getAudioSubsystemImpl(_audio_subsystem);
    audio_impl->_onAudioInit = [this]() {
      //logchan_ezapp->log("Audio subsystem triggering audio init");
      _audioInit();
    };
    audio_impl->_onAudioExit = [this]() {
      //logchan_ezapp->log("Audio subsystem triggering audio cleanup");
      _audioExit();
    };

    subsystem_map["audio"] = _audio_subsystem;
  }

  // Lev2 meta-service (optional)
  if (want("lev2")) {
    auto lev2_subsystem = createLev2Subsystem();
    if (want("gpu")) {
      lev2_subsystem->_pending_children.push_back("gpu");
    }
    if (_initdata->_enable_audio) {
      lev2_subsystem->_pending_children.push_back("audio");
    }
    // lev2 always depends on core - must wait for core to finish before starting
    lev2_subsystem->_pending_dependencies.push_back("core");
    // lev2 must run on main thread because its children (gpu) require main thread
    subsystem_map["lev2"] = lev2_subsystem;
  }

  // Add custom subsystems from Python
  for (auto& custom_sub : custom_subsystems) {
    subsystem_map[custom_sub->_name] = custom_sub;
    logchan_ezapp->log("Added custom subsystem: %s", custom_sub->_name.c_str());
  }

  /////////////////////////////////////////////
  // Resolve pending dependencies and children to actual pointers
  /////////////////////////////////////////////

  for (auto& [name, subsystem] : subsystem_map) {
    // Resolve dependencies
    for (auto& dep_name : subsystem->_pending_dependencies) {
      auto it = subsystem_map.find(dep_name);
      if (it != subsystem_map.end()) {
        subsystem->addDependency(it->second);
        //logchan_ezapp->log("  %s depends on %s", name.c_str(), dep_name.c_str());
      } else {
        logchan_ezapp->log("WARNING: %s has unresolved dependency: %s", name.c_str(), dep_name.c_str());
      }
    }

    // Resolve children and set parent pointers
    for (auto& child_name : subsystem->_pending_children) {
      auto it = subsystem_map.find(child_name);
      if (it != subsystem_map.end()) {
        auto& child = it->second;
        subsystem->addChild(child);
        child->_parent = subsystem;  // Set parent pointer
        //logchan_ezapp->log("  %s has child %s", name.c_str(), child_name.c_str());
      } else {
        logchan_ezapp->log("WARNING: %s has unresolved child: %s", name.c_str(), child_name.c_str());
      }
    }
  }

  /////////////////////////////////////////////
  // Register all subsystems
  /////////////////////////////////////////////

  for (auto& [name, subsystem] : subsystem_map) {
    registerSubsystem(subsystem, true);
  }

  /////////////////////////////////////////////
  // Initialize ROOT subsystems only (those with no parent)
  // Non-root subsystems (children) are initialized by their parent's initChildren()
  //
  // Thread affinity:
  //   "" = don't care (can run in parallel on any thread)
  //   "main" = must run on main thread (GPU, GLFW, UI)
  //   "audio" = must run on audio thread (not available at init time)
  //   "update" = must run on update thread (not available at init time)
  //
  // Current implementation: Sequential init on main thread
  // Subsystems with _requires_thread="main" MUST init on main thread
  // Subsystems with _requires_thread="" CAN be parallelized (future optimization)
  /////////////////////////////////////////////

  // Build list of root subsystems (those with no parent)
  std::vector<subsystem_ptr_t> root_subsystems;
  for (auto& [name, subsystem] : subsystem_map) {
    if (!subsystem->hasParent()) {
      root_subsystems.push_back(subsystem);
    } else {
      logchan_ezapp->log("  %s is a child (parent: %s), will be initialized by parent",
                         name.c_str(), subsystem->parent()->_name.c_str());
    }
  }

  // Initialize root subsystems in dependency order using shared utility
  initSubsystemsInOrder(root_subsystems);

  logchan_ezapp->log("HFSM subsystems registered and initialized");
}

///////////////////////////////////////////////////////////////////////////////
// Initialize graphics context (called by GPU subsystem or directly in ad-hoc mode)
///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::_initGraphicsContext() {
#if defined(__linux__)
  // Handle --drm-list option
  if (_initdata->_miscvars.find("drm-list") != _initdata->_miscvars.end()) {
    auto& drm_list_var = _initdata->_miscvars.at("drm-list");
    if (drm_list_var.isA<bool>() && drm_list_var.get<bool>()) {
      drm::DRMContext::listMonitorsAndExit();
    }
  }
#endif

  //logchan_ezapp->log("initializing graphics context");
  //fflush(stdout);
  _appstate = 0;

  _uicontext = std::make_shared<ui::Context>();

  //////////////////////////////////////////////

  _mainWindow = std::make_shared<EzMainWin>(*this);

  //////////////////////////////////////
  // create lev2 gfxwindow
  //////////////////////////////////////
  _mainWindow->_appwin           = std::make_shared<AppWindow>(nullptr);
  _mainWindow->_appwin->miWidth  = _initdata->_width;
  _mainWindow->_appwin->miHeight = _initdata->_height;
  GfxEnv::GetRef().RegisterWinContext(_mainWindow->_appwin.get());
  //////////////////////////////////////
  //////////////////////////////////////
  _eztopwidget = std::make_shared<EzTopWidget>(_mainWindow.get());
  if (_initdata->_disableMouseCursor) {
    _eztopwidget->_clipEvents = false;
  }
  _eztopwidget->_uicontext          = _uicontext.get();
  _mainWindow->_appwin->_rootWidget = _eztopwidget;
  _eztopwidget->_topLayoutGroup =
      _uicontext->makeTop<ui::LayoutGroup>("ezapp-top-layoutgroup", 0, 0, _initdata->_width, _initdata->_height);
  _topLayoutGroup = _eztopwidget->_topLayoutGroup;
  if (_initdata->_disableMouseCursor) {
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

  //logchan_ezapp->log("graphics context initialized");
}

///////////////////////////////////////////////////////////////////////////////

void OrkEzApp::joinUpdate() {
  uint64_t prevappsate = _appstate.fetch_or(KAPPSTATEFLAG_JOINING);
  ////////////////////////////////////////////////
  bool has_joined_already = bool(prevappsate & KAPPSTATEFLAG_JOINING);
  ////////////////////////////////////////////////
  if (not has_joined_already) {
    //logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:1", this);
    for( int i=0; i<100; i++ ) {
      //checkAppState(KAPPSTATEFLAG_UPDRUNNING)) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
    }
    //logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:2", this);
    _update_queue->drain();
    _updateThread.join();
    //logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:3", this);
    DrawQueue::ClearAndSyncWriters();
    //logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:4", this);
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
void OrkEzApp::onEzAppInit(void_lambda_t callback) {
  _onEzAppInit = callback;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::onEzAppExit(void_lambda_t callback) {
  _onEzAppExit = callback;
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
  // Wire to uicontext's fallback handler so events unhandled by widgets
  // will still reach the application-level handler
  if (_uicontext) {
    //_uicontext->_appFallbackHandler = cb;
  }
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
  static std::atomic<int> audiodevice_instance_count = 0;
  //logchan_ezapp->log("OrkEzApp::_audioInit");
  if( audiodevice_instance_count.fetch_add(1) > 0 ) {
    return;
  }
  _audiodevice = AudioDevice::createInstance(_initdata);
  _initdata->_miscvars["audiodevice"].set<audiodevice_ptr_t>(_audiodevice);
  if (_initdata->_enable_audio_synth) {
    _synth = audio::singularity::synth::instance();
    _initdata->_miscvars["synth"].set<audio::singularity::synth_ptr_t>(_synth);
    if (_synth) {
      _synth->mainThreadHandler();
    }
  }
  // Callbacks are always deferred to _fireDeferredAudioCallbacks() which is called
  // at the start of mainThreadLoop(), after Python has registered its callbacks.
  _audiodevice->startup();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_fireDeferredAudioCallbacks() {
  // Called at the start of mainThreadLoop(), after callbacks are registered.
  // This works for both ad-hoc and subsystem modes.
  if (_synth && _onSynthInit) {
    _onSynthInit(_synth);
  }
  if (_audiodevice && _onAudioInit) {
    _onAudioInit(_audiodevice);
  }
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

  ////////////////////////////////////////
  // Update Thread Implementation
  ////////////////////////////////////////

  _update_thread_impl = [&](anyp data) {

    ork::SetCurrentThreadName("update");
    opq::TrackCurrent opqtest(_update_queue);

    float target_ups = _initdata->_target_ups;
    float target_fps = _initdata->_target_fps;

    // first time init ?
    if (_mainWindow && _mainWindow->_onUpdateInit)
      _mainWindow->_onUpdateInit();

    _appstate.fetch_or(KAPPSTATEFLAG_UPDRUNNING);

    ////////////////////////////////////////
    // FREERUNNING MODE: Wall clock, existing behavior
    ////////////////////////////////////////

    if (_initdata->_freerunning) {
      logchan_ezapp->log("FREERUNNING MODE: realtime, tgt UPS<%g> tgt FPS<%g>", target_ups, target_fps);

      AdaptiveWait wait{AdaptiveWait::Mode::Precise};
      double step_time    = 1.0 / target_ups;
      u64 step_ticks      = u64(double(NS_PER_SEC) / double(target_ups)); // ticks per update step
      u64 prev_tick       = Timer::getSystemTick();
      u64 step_grid_tick  = prev_tick + step_ticks; // absolute grid: advances by exactly step_ticks each frame

      ////////////////////////////////////////
      // Freerun Update Loop
      ////////////////////////////////////////

      while (not checkAppState(KAPPSTATEFLAG_JOINING)) {
        OrkProfilerFrameBegin(CHANNEL_UPDATE, CpuProfilerChannel, {.capture_fps = true});

        {
          OrkProfilerSampleScope(CHANNEL_UPDATE, SERIES_EZAPP_UPDATE_FREERUN);

          // Log +/- error of ticks from target. 
          // Can also look in profilerview at UpdateThread FPS to see how locked it is on the target.
          if (0) {
            u64 now_tick    = Timer::getSystemTick();
            u64 delta_ticks = now_tick - prev_tick;
            prev_tick = now_tick;
            s64 delta_error_ticks = (s64)delta_ticks - (s64)step_ticks;
            printf("Update sleep error ticks: %lld\n", delta_error_ticks);
          }

          bool do_update = _mainWindow && bool(_mainWindow->_onUpdate);
          if (do_update) {
            _update_data->_dt       = step_time;
            _update_data->_abstime += step_time;
            _update_data->_counter  = _update_count.load();
            if(0) printf("OrkEzApp<%p> update dt<%g> abstime<%g> count<%d>\n", this, _update_data->_dt, _update_data->_abstime, (int)_update_data->_counter);
            
            if (not checkAppState(KAPPSTATEFLAG_JOINING)) {
              if (_mainWindow->_onUpdateInternal)
                _mainWindow->_onUpdateInternal(_update_data);

              if (_mainWindow->_onUpdate)
                _mainWindow->_onUpdate(_update_data);
              else if (_mainWindow->_onUpdateWithScene)
                _mainWindow->_onUpdateWithScene(_update_data, _mainWindow->_execscene);

              _update_count.fetch_add(1);
            }
          }

          opq::updateSerialQueue()->Process();
        }
        
        // Wait till next step on the absolute grid (self-corrects overshoot each frame)
        wait.sleepUntilTick(step_grid_tick);
        step_grid_tick += step_ticks;

        // Frame end after wait so FPS display in profiler will show the waited FPS.
        // Then you can see how closely it stays at precisely the target FPS.
        OrkProfilerFrameEnd(CHANNEL_UPDATE);

      } // while (not checkAppState(KAPPSTATEFLAG_JOINING)) {

    } // end async mode

    ////////////////////////////////////////
    // SYNCHRONOUS MODE: Virtual time, deterministic
    ////////////////////////////////////////

    else {
      
      logchan_ezapp->log("LockStep/Synchronous MODE: UPS=%g FPS=%g", target_ups, target_fps);

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
      double render_timeaccumulator = 0.0;

      ////////////////////////////////////////
      // Synchronous Update Loop
      ////////////////////////////////////////

      while (not checkAppState(KAPPSTATEFLAG_JOINING)) {
        OrkProfilerFrameBegin(CHANNEL_UPDATE, CpuProfilerChannel);
        OrkProfilerSampleBegin(CHANNEL_UPDATE, SERIES_EZAPP_UPDATE_LOCKSTEP);

        // Fixed time step per update
        virtual_time += update_delta;
        render_timeaccumulator += update_delta;

        // Run update
        bool do_update = _mainWindow && bool(_mainWindow->_onUpdate);
        if (do_update) {
          _update_data->_dt      = update_delta;
          _update_data->_abstime = virtual_time;
          _update_data->_counter = _update_count.load();
          if(0) printf( "OrkEzApp<%p> update dt<%g> abstime<%g> count<%d>\n", this, _update_data->_dt, _update_data->_abstime, (int)_update_data->_counter );
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
          opq::auxSerialQueue()->enqueue(op);
        }

        // Check if we should render a frame
        while (render_timeaccumulator >= frame_delta) {
          _lockstep_frame_requests.fetch_add(1);
          render_timeaccumulator -= frame_delta;
        }
        while ((not checkAppState(KAPPSTATEFLAG_JOINING)) and (_lockstep_frame_requests.load() > 0)) {
          sched_yield();
        }

        opq::updateSerialQueue()->Process();

        OrkProfilerSampleEnd(CHANNEL_UPDATE, SERIES_EZAPP_UPDATE_LOCKSTEP);
        OrkProfilerFrameEnd(CHANNEL_UPDATE);
      } // while (not checkAppState(KAPPSTATEFLAG_JOINING)) {

    } // end sync mode

    _appstate.fetch_or(KAPPSTATEFLAG_JOINED);
    _appstate.fetch_and(~KAPPSTATEFLAG_UPDRUNNING);

    if (_mainWindow && _mainWindow->_onUpdateExit) {
      _mainWindow->_onUpdateExit();
    }

    // Only call _audioExit() directly in legacy mode
    // In subsystem mode, audio shutdown is handled by the audio subsystem's HFSM
    // which ensures proper thread coordination (shutdown on main thread)
    if (!_initdata->_use_subsystems) {
      _audioExit();
    }
  };

  if (not _mainWindow) {
    while (this->_onRunLoopIteration) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
      // Process synth main thread tasks (sequencer, HUD events, etc.)
      if (_synth) {
        _synth->mainThreadHandler();
      }
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
    //logchan_ezapp->log("BEGIN OrkEzApp::_onGpuInit");
    context->beginPrimaryCommandBuffer();
    //logchan_ezapp->log("_initdata->_enable_audio<%d>", (int)_initdata->_enable_audio);

    if (_ginitdata->_disableMouseCursor) {
      auto ctxbase = context->GetCtxBase();
      ctxbase->disableMouseCursor();
    }

    // Only init audio here in legacy mode - subsystem mode already initialized it
    if (_initdata->_enable_audio && !_initdata->_use_subsystems) {
      _audioInit();
    }

    if (_mainWindow->_onGpuInit) {
      _mainWindow->_onGpuInit(context);
    }
    context->endPrimaryCommandBuffer();

    //logchan_ezapp->log("END OrkEzApp::_onGpuInit");
    logchan_ezapp->log("starting update thread...");
    _update_thread.start(_update_thread_impl);
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

    // CRITICAL FIX: Also process gloadercontext frames when window exists
    // This is needed for command-line apps that create a hidden window for Vulkan
    // The ContextExecutor uses gloadercontext, so we must pump frames on it
    if (gloadercontext) {
      gloadercontext->beginFrame(false);
      gloadercontext->endFrame();
    }
  } else {
    gloadercontext->beginFrame(false);
    gloadercontext->endFrame();
  }

  // Phase 5: Render secondary windows and cleanup closed ones
  _renderSecondaryWindows();
  _cleanupClosedSecondaryWindows();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_mainThreadLoopEnd() {
  // Close all secondary windows before shutting down
  closeAllSecondaryWindows();
  _secondaryWindows.clear();

  if (_mainWindow) {
    auto ctx = _mainWindow->_ctqt;
    ctx->_runloopEnd();
  }
  size_t num_prof_blocks = profiler::dumpBlocksToFile("test_profile.prof");
  logchan_ezapp->log( "Dumped %zu profiler blocks to test_profile.prof\n", num_prof_blocks);
}
///////////////////////////////////////////////////////////////////////////////
int OrkEzApp::mainThreadLoop() {
  // Fire deferred audio callbacks now that all callbacks are registered
  _fireDeferredAudioCallbacks();

  {
    OrkProfilerFrameBegin(CHANNEL_MAIN, CpuProfilerChannel, {.capture_fps = true});
    _mainThreadLoopBegin();
    OrkProfilerFrameEnd(CHANNEL_MAIN);
  }

  // Wall-clock FPS tracking
  ork::Timer fps_timer;
  fps_timer.Start();
  double frame_count = 0.0;
  double max_frame_time = 0.0;  // Track max frame time in current window (seconds)
  ork::Timer frame_timer;

  if (_mainWindow) {

    auto ctx = _mainWindow->_ctqt;
    if (_initdata->_freerunning) {

      ////////////////////////////////////////
      // Freerun Main Thread
      //  Synchronization controlled by gfx acquire.
      ////////////////////////////////////////

      while (ctx->_runstate == 1) {
        OrkProfilerFrameBegin(CHANNEL_MAIN, CpuProfilerChannel, {.capture_fps = true});
        OrkProfilerSampleBegin(CHANNEL_MAIN, SERIES_EZAPP_MAIN_FREERUN);

        frame_timer.Start();  // Start timing this frame

        // Process synth main thread tasks (sequencer, HUD events, etc.)
        if (_synth) {
          OrkProfilerSampleScope(CHANNEL_MAIN, "ez:audio_synth");
          _synth->mainThreadHandler();
        }

        {
          OrkProfilerSampleScope(CHANNEL_MAIN, "ez:run_loop");
          ctx->_runloopIter(true);
        }

        {
          OrkProfilerSampleScope(CHANNEL_MAIN, "ez:secondary_windows");
          // Render secondary windows
          _renderSecondaryWindows();
          _cleanupClosedSecondaryWindows();
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

        OrkProfilerSampleEnd(CHANNEL_MAIN, SERIES_EZAPP_MAIN_FREERUN);
        OrkProfilerFrameEnd(CHANNEL_MAIN);
      }

    } else {

      ////////////////////////////////////////
      // SYNCHRONOUS MAIN THREAD
      ////////////////////////////////////////

      while (ctx->_runstate == 1) {

        while (_lockstep_frame_requests.load()) {
          OrkProfilerFrameBegin(CHANNEL_MAIN, CpuProfilerChannel, {.capture_fps = true});
          OrkProfilerSampleBegin(CHANNEL_MAIN, SERIES_EZAPP_MAIN_LOCKSTEP);

          // Process synth main thread tasks (sequencer, HUD events, etc.)
          if (_synth) {
            OrkProfilerSampleScope(CHANNEL_MAIN, "ez:audio_synth");
            _synth->mainThreadHandler();
          }

          {
            OrkProfilerSampleScope(CHANNEL_MAIN, "ez:run_loop");
            ctx->_runloopIter(false);
          }

          {
            OrkProfilerSampleScope(CHANNEL_MAIN, "ez::secondary_windows");
            // Render secondary windows
            _renderSecondaryWindows();
            _cleanupClosedSecondaryWindows();
          }

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

          OrkProfilerSampleEnd(CHANNEL_MAIN, SERIES_EZAPP_MAIN_LOCKSTEP);
          OrkProfilerFrameEnd(CHANNEL_MAIN);
        }
        sched_yield();
      }

    }
  }
  else{
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
// Phase 4: Secondary window support
///////////////////////////////////////////////////////////////////////////////

ezsecondarywin_ptr_t OrkEzApp::createSecondaryWindow(const EzSecondaryWinConfig& config) {
  auto win = std::make_shared<EzSecondaryWin>(config);
  _secondaryWindows.push_back(win);
  return win;
}

void OrkEzApp::closeSecondaryWindow(ezsecondarywin_ptr_t win) {
  if (win) {
    win->requestClose();
  }
}

void OrkEzApp::closeAllSecondaryWindows() {
  for (auto& win : _secondaryWindows) {
    win->requestClose();
  }
}

void OrkEzApp::_renderSecondaryWindows() {
  for (auto& win : _secondaryWindows) {
    if (!win->shouldClose() && win->needsRender()) {
      win->_render();
    }
  }
}

void OrkEzApp::_cleanupClosedSecondaryWindows() {
  // Remove closed windows and return focus to main window if any were removed
  size_t before = _secondaryWindows.size();

  // Force-close GLFW windows before erasing shared_ptrs.
  // Python callbacks may hold circular references to the window object,
  // preventing the destructor from running. Explicit close ensures the
  // GLFW window is destroyed regardless of reference counting.
  for (auto& w : _secondaryWindows) {
    if (w->shouldClose()) {
      w->_forceClose();
    }
  }

  std::erase_if(_secondaryWindows, [](const auto& w) {
    return w->shouldClose();
  });

  size_t removed = before - _secondaryWindows.size();
  if (removed > 0) {
    logchan_ezapp->log("Removed %zu closed secondary window(s)", removed);
  }

  // Return focus to main window if any popups were closed
  if (_secondaryWindows.size() < before) {
#if defined(ENABLE_GLFW)
    if (_mainWindow && _mainWindow->_ctqt) {
      auto ctx = dynamic_cast<CtxGLFW*>(_mainWindow->_ctqt);
      if (ctx && ctx->_glfwWindow) {
        glfwFocusWindow(ctx->_glfwWindow);
      }
    }
#endif
  }
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

ork::lev2::orkezapp_ptr_t lev2appinit(ork::appinitdata_ptr_t init_data) {
  ork::SetCurrentThreadName("main");

  ork::genviron.init_from_global_env();

  static auto _init_data = init_data;
  if (_init_data == nullptr) {
    _init_data = ::ork::appinitdata();
  }

  _init_data->_offscreen = true;
  auto ezapp             = ork::lev2::OrkEzApp::create(_init_data);

  ork::lev2::initModule(init_data);

  static std::shared_ptr<ork::lev2::ThreadGfxContext> _gthreadgfxctx;
  _gthreadgfxctx = std::make_shared<ork::lev2::ThreadGfxContext>(ork::lev2::gloadercontext.get());

    if(_init_data->_enable_graphics ){
        ork::lev2::gloadercontext->makeCurrentContext();

    }
    init_data->finalizeInitialization();

  return ezapp;
}
