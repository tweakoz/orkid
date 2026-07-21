#include <ork/lev2/ezapp.h>
#include <ork/util/debugserver.h>
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
#if defined(__APPLE__)
int64_t nativeCocoaWindowNumber(GLFWwindow* window); // ctx_glfw_osx.mm
#endif

static logchannel_ptr_t logchan_ezapp = logger()->configureChannel("EZAPP", fvec3(0.7, 0.7, 0.9), true);

// Loader thread is now owned by lev2_init.cpp (auto-spawned at
// gloadercontext creation, stopped via ork::lev2::stopLoaderThread()).

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
  // opt-in in-process debug console (ORKID_DEBUG_SERVER=<sessionid>); query it with
  // ork.debug.lldbclient.py --debugserver --sessionid <sid> threads
  debugserver::startFromEnv();
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
              ("displaylink", po::bool_switch()->default_value(false), "use CVDisplayLink/Metal swapchain for VR timing (fullscreen only, Apple)") //
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
int OrkEzApp::addGlobalEventHandler(globalevcb_t cb) {
  std::lock_guard<std::mutex> lk(_globalHandlerMutex);
  int id = _nextGlobalHandlerID++;
  _globalEventHandlers.emplace(id, std::move(cb));
  return id;
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::removeGlobalEventHandler(int token) {
  std::lock_guard<std::mutex> lk(_globalHandlerMutex);
  _globalEventHandlers.erase(token);
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_fireGlobalEvent(ui::event_constptr_t ev) {
  // Snapshot under lock so handlers can safely add/remove during dispatch
  std::vector<globalevcb_t> snapshot;
  {
    std::lock_guard<std::mutex> lk(_globalHandlerMutex);
    if (_globalEventHandlers.empty())
      return;
    snapshot.reserve(_globalEventHandlers.size());
    for (auto& kv : _globalEventHandlers) {
      snapshot.push_back(kv.second);
    }
  }
  for (auto& h : snapshot) {
    try {
      h(ev);
    } catch (const std::exception& e) {
      logchan_ezapp->log("global event handler threw: %s", e.what());
    } catch (...) {
      logchan_ezapp->log("global event handler threw unknown exception");
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
static bool _isPointerInjectCode(ui::EventCode c) {
  switch (c) {
    case ui::EventCode::PUSH:
    case ui::EventCode::RELEASE:
    case ui::EventCode::MOVE:
    case ui::EventCode::DRAG:
      return true;
    default:
      return false;
  }
}
void OrkEzApp::injectUiEvent(ui::event_ptr_t ev) {
  auto uictx = _uicontext;
  if (not uictx)
    OrkAssert(false); // injectUiEvent: no ui context — inject after onGpuInit
  auto root = uictx->_top;
  if (not root)
    OrkAssert(false); // injectUiEvent: ui context has no top widget — inject after onGpuInit
  // Mirror the engine's single shared mutable Event (ctx_glfw trap #1): pointer events
  // carry their own position; key/wheel events inherit the last injected pointer position
  // so position-based routing (Context::routeUiEvent is a hit-test on miX/miY) reaches the
  // last-hovered widget — exactly as a real KEY reuses the shared event's cursor position.
  if (_isPointerInjectCode(ev->_eventcode)) {
    _injectLastX = ev->miX;
    _injectLastY = ev->miY;
  } else {
    ev->miX = _injectLastX;
    ev->miY = _injectLastY;
  }
  ev->_uicontext = uictx.get();
  ev->setvpDim(root.get());
  _fireGlobalEvent(ev);
  ui::Event::sendToContext(ev);
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
OrkEzApp* OrkEzApp::currentRaw() { return __priv_gapp.load(); }
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
    , _update_thread("updatethread") {

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
  // Stop the loader thread before any context/window teardown — it's
  // pumping gloadercontext->beginFrame at 500µs and would touch a dying
  // context. Idempotent; safe if already stopped via gpu _onGpuExit.
  stopLoaderThread();
  if (_mainWindow) {
    DrawQueue::terminateAll();
  }
  __priv_gapp.store(nullptr);
}

///////////////////////////////////////////////////////////////////////////////
// VR windowless policy. When the ACTIVE VR device owns HMD presentation (it imports
//  its own swapchain from the XR runtime and presents to the headset directly), the
//  app must NOT open an on-screen window — there is no surface to create, and building
//  a window-present swapchain would collide with the presentation the runtime owns.
//  Forcing _offscreen routes BOTH the platform window (hidden) and the main context
//  (no presentation surface) down the already-proven offscreen branch, so the per-frame
//  render tick — which drives the compositor's VR output node (xrWaitFrame/xrBeginFrame
//  in gpuUpdate, xrEndFrame in __composite) — keeps firing exactly as it would with a
//  window. Gated on _active so a VR device whose runtime was absent (preGraphicsInit
//  cleared _active) stays windowed. A device that does not own HMD presentation (NoVR,
//  desktop preview) is untouched. Must run AFTER graphics init settled the device state
//  (ensureLoaderContext → postGraphicsInit) and BEFORE the window is created.
///////////////////////////////////////////////////////////////////////////////

static void _applyVrWindowlessPolicy(appinitdata_ptr_t initdata) {
  auto vrdev = orkidvr::device();
  if (vrdev and vrdev->_active and vrdev->ownsHmdPresentation() and not initdata->_offscreen) {
    initdata->_offscreen = true;
    logchan_ezapp->log("VR device owns HMD presentation — main context is WINDOWLESS (offscreen; no window, no present surface)");
  }
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

    // VR that owns HMD presentation → windowless main context (before window creation).
    _applyVrWindowlessPolicy(_initdata);

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
      // ensureLoaderContext() creates gloadercontext and auto-spawns the
      // loader thread (in lev2_init.cpp). Nothing extra needed here.
      auto loader_ctx = ensureLoaderContext();
      logchan_ezapp->log("GPU subsystem loader context: %p", (void*)loader_ctx.get());
      _initGraphicsContext();
    };
    gpu_impl->_onGpuExit = [this]() {
      logchan_ezapp->log("GPU subsystem triggering graphics cleanup");
      // Stop the loader thread BEFORE the gpu subsystem tears down
      // gloadercontext — otherwise the thread pumps a dying context.
      stopLoaderThread();
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
      if(0)logchan_ezapp->log("  %s is a child (parent: %s), will be initialized by parent",
                         name.c_str(), subsystem->parent()->_name.c_str());
    }
  }

  // Initialize root subsystems in dependency order using shared utility
  initSubsystemsInOrder(root_subsystems);

  if(0)logchan_ezapp->log("HFSM subsystems registered and initialized");
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

  // VR that owns HMD presentation → windowless main context. Safe here: this runs from
  //  the GPU subsystem's _onGpuInit, AFTER ensureLoaderContext()/postGraphicsInit have
  //  settled the device state, and BEFORE the window is created just below.
  _applyVrWindowlessPolicy(_initdata);

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

void OrkEzApp::joinUpdate(Context* ctx) {
  uint64_t prevappsate = _appstate.fetch_or(KAPPSTATEFLAG_JOINING);
  ////////////////////////////////////////////////
  bool has_joined_already = bool(prevappsate & KAPPSTATEFLAG_JOINING);
  ////////////////////////////////////////////////
  if (not has_joined_already) {
    // JOINS MUST PUMP (the LOADX §1.1 principle, shutdown edition): the update thread
    // may be blocked inside a GPU-phase rendezvous (Simulation::_runGpuPhaseOnRenderThread
    // future.wait) that only the gpu-update chain services — and the render loop that
    // used to invoke that chain has already exited when we get here. A bare join
    // deadlocked EVERY offscreen/materialize exit (main: joinUpdate/Thread::join;
    // update: __assoc_sub_state::wait — sampled 2026-07-02). So keep servicing _mainq
    // AND the gpu-update hook until the update thread has actually left its loop.
    while (checkAppState(KAPPSTATEFLAG_UPDRUNNING)) {
      {
        opq::TrackCurrent opqtest(_mainq);
        _mainq->Process();
      }
      if (ctx and _mainWindow and _mainWindow->_onGpuUpdate)
        _mainWindow->_onGpuUpdate(ctx);
      ork::usleep(1000);
    }
    //logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:1", this);
    for( int i=0; i<100; i++ ) {
      opq::TrackCurrent opqtest(_mainq);
      _mainq->Process();
    }
    //logger()->defaultChannel()->log("OrkEzApp<%p> joinUpdate:2", this);
    // bounded: the update thread services its queue as its final act, so this
    // is normally instant — but an op enqueued after that last service (or an
    // op that itself re-enqueues) must not wedge shutdown forever.
    _update_queue->drain(5.0f);
    _update_thread.join();
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

    // UPDRUNNING must be live BEFORE _onUpdateInit: the init runs the scene
    // load, which rendezvouses with the gpu/main threads, and joinUpdate()'s
    // pump loop keys on this flag — raised any later, an exit requested
    // mid-load skips the pump and bare-joins a blocked loader (Ctrl-C wedge).
    _appstate.fetch_or(KAPPSTATEFLAG_UPDRUNNING);

    // first time init ?
    if (_mainWindow && _mainWindow->_onUpdateInit)
      _mainWindow->_onUpdateInit();

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
            printf("Update sleep error ticks: %ld\n", delta_error_ticks);
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

    // FINAL update-serial-queue service. When an exit request pre-empts the
    // update loop entirely (Ctrl-C mid-load), ops enqueued during the load —
    // and by _onUpdateExit just above — have never been processed, and this
    // thread is their only legal processor. Leaving them queued wedges
    // joinUpdate()'s drain on the main thread.
    opq::updateSerialQueue()->Process();
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

    // A fresh main render context is coming up — clear any shutdown latch left
    // by a prior app lifetime (re-init safety). Normal runtime keeps it false.
    GfxEnv::setGpuShutdownComplete(false);

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
    // Wind down the background loader thread FIRST, while every GPU context /
    // Vulkan device / driver library is still fully live. It pumps
    // gloadercontext->beginFrame/endFrame (a real queueSubmit) at 500us; if it
    // is left running past this funnel it races the device teardown that follows
    // (subsystem shutdown(), glfwDestroyWindow, static destruction) and submits
    // into a torn-down driver -> VkThreadedQueue::queueSubmit dereferences a null
    // driver entrypoint -> SIGSEGV on the "loader" thread AFTER all output (the
    // NVIDIA bare-EzApp exit crash). ~OrkEzApp / ~LoaderThread also stopLoaderThread(),
    // but those run too late (Python finalization) — after shutdown() already freed
    // the device. This is the orderly point the headless (ecs.headless_exit) path
    // uses; do the same here. Idempotent.
    stopLoaderThread();
    joinUpdate(context);
    if (_moviecapcontext) {
      _moviecapcontext->terminate();
    }

    if (_mainWindow->_onGpuExit) {
      _mainWindow->_onGpuExit(context);
  }
    // Release GPU state owned by process-lifetime singletons NOW, on the render
    // thread with the context still live, so their destructors don't fire at
    // atexit against a freed Context. FontMan is the known reproducer.
    FontMan::gpuExit(context);
    // Orderly GPU teardown funnel: scene/app gpuExit has run against the still
    // live context (real vkDestroy*), and no more frames will be drawn. Latch
    // GPU-shutdown so mainRenderContext() reports null and any GPU-resource
    // destructor that fires later during static teardown no-ops rather than
    // dereferencing the Context that OrkEzApp teardown is about to free.
    GfxEnv::setGpuShutdownComplete(true);
  };
  ctx->_runloopBegin();
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_mainThreadLoopIter() {
  OrkProfilerFrameBegin(CHANNEL_MAIN, CpuProfilerChannel, {.capture_fps = true});

  if (_mainWindow) {
    auto ctx = _mainWindow->_ctqt;
    ctx->_runloopIter();
  }

  // Phase 5: Render secondary windows and cleanup closed ones
  _renderSecondaryWindows();
  _cleanupClosedSecondaryWindows();

  OrkProfilerFrameEnd(CHANNEL_MAIN);
}
///////////////////////////////////////////////////////////////////////////////
void OrkEzApp::_mainThreadLoopEnd() {
  // Close all secondary windows before shutting down
  closeAllSecondaryWindows();
  _secondaryWindows.clear();

  // Release any persistent main-thread TLS pinned by bindGfxToCurrentThread.
  // Must come before _runloopEnd which pushes its own stack-scoped tracker.
  unbindGfxFromCurrentThread();

  if (_mainWindow) {
    auto ctx = _mainWindow->_ctqt;
    ctx->_runloopEnd();
  }
  size_t num_prof_blocks = profiler::dumpBlocksToFile("test_profile.prof");
  logchan_ezapp->log( "Dumped %zu profiler blocks to test_profile.prof\n", num_prof_blocks);
}

///////////////////////////////////////////////////////////////////////////////
// Headless-friendly TLS attach: pin the main window's gfx context to the
// calling thread for the lifetime of the script (until mainThreadEnd or an
// explicit unbind). Required for inline GPU work from Python — without
// this, contextForCurrentThread() is null between _runloopIter calls.
// Idempotent.

Context* OrkEzApp::mainGfxContext() const {
  if (!_mainWindow || !_mainWindow->_ctqt) return nullptr;
  return _mainWindow->_ctqt->_target;
}

void OrkEzApp::bindGfxToCurrentThread() {
  if (_persistent_main_tls) return;
  auto target = mainGfxContext();
  OrkAssert(target && "bindGfxToCurrentThread called before main gfx context exists "
                      "(must come after mainThreadBegin)");
  _persistent_main_tls = std::make_unique<ThreadGfxContext>(target);
}

void OrkEzApp::unbindGfxFromCurrentThread() {
  _persistent_main_tls.reset();
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

      ////////////////////////////////////////
      // Frame-pacing decision (one-shot). The freerun render loop must be gated by exactly
      // ONE pacer. Whenever the display/output path already owns a blocking (or display-
      // locked) sync primitive — the XR runtime's xrWaitFrame, a FIFO/vsync swapchain, the
      // Apple race-the-beam scanout sleep, or DRM vblank — the loop rides that alone;
      // layering an internal governor on top would beat against it (aperiodic period
      // skips/doubles → pose-time jumps in VR). The internal target-fps governor paces the
      // loop ONLY when nothing else gates it (a real on-screen surface with a non-blocking
      // present). Headless/offscreen (no display, no XR) is left ungated and SILENT — the
      // battery/self-test paths stay byte-identical. NB: the update thread's own pacing
      // (below, 480 UPS grid) is independent and untouched.
      ////////////////////////////////////////

      auto pace_ctx   = mainGfxContext();
      auto pace_vrdev = orkidvr::device();
      bool xr_owns    = pace_vrdev and pace_vrdev->_active and pace_vrdev->ownsHmdPresentation();
      bool vsync_paced      = (not xr_owns) and pace_ctx and pace_ctx->displayProvidesFramePacing();
      bool internal_governor = (not xr_owns) and (not vsync_paced) and (not _initdata->_offscreen);

      const double gov_fps = (_initdata->_target_fps > 0.0) ? double(_initdata->_target_fps) : 60.0;
      AdaptiveWait gov_wait{AdaptiveWait::Mode::Precise};
      u64 gov_step_ticks = u64(double(NS_PER_SEC) / gov_fps);
      u64 gov_grid_tick  = Timer::getSystemTick() + gov_step_ticks;

      if (xr_owns)
        logchan_ezapp->log("frame pacing: xr-runtime — XR runtime owns presentation; internal render governor disabled.");
      else if (vsync_paced)
        logchan_ezapp->log("frame pacing: vsync-present — blocking/display-locked present owns render cadence; internal render governor disabled.");
      else if (internal_governor)
        logchan_ezapp->log("frame pacing: internal governor (tgt %g fps) — no display sync primitive.", gov_fps);
      // else: offscreen/headless, no on-screen pacer to name — stays silent (byte-identical).

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

        // TODO change is so OrkProfilerFrameBegin can forward it's 'capture_fps' value logchannel
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

        // Internal frame governor — engaged ONLY when nothing else gates the loop (no XR
        // runtime, no vsync/blocking present, real on-screen surface). Absolute-grid wait
        // self-corrects overshoot; sleepUntilTick(past) renders ASAP when behind.
        if (internal_governor) {
          gov_wait.sleepUntilTick(gov_grid_tick);
          gov_grid_tick += gov_step_ticks;
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
  // subsystem lifecycle: the HFSM teardown runs AUTOMATICALLY when the loop
  // ends, so every subsystem-based app exits clean without remembering an
  // explicit call. Application::shutdown() is idempotent (atomic flag), so an
  // earlier or later explicit shutdown() is harmless. (Called GIL-free: the
  // pyext mainThreadLoop binding releases the GIL around this whole loop.)
  if (_initdata and _initdata->_use_subsystems)
    shutdown();
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

bool OrkEzApp::mainWindowScreenRect(int& x, int& y, int& w, int& h) {
  x = 0; y = 0; w = 0; h = 0;
  if (not _mainWindow)
    return false;
  auto ctxbase = dynamic_cast<CtxGLFW*>(_mainWindow->_ctqt);
  if (not ctxbase || not ctxbase->_glfwWindow)
    return false;
  // Wayland cannot report a global window position — degrade (contract in
  // dock_coordinator.h).
  if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
    return false;
  glfwGetWindowPos(ctxbase->_glfwWindow, &x, &y);
  glfwGetWindowSize(ctxbase->_glfwWindow, &w, &h);
  return w > 0 && h > 0;
}

int64_t OrkEzApp::mainWindowNativeNumber() {
#if defined(__APPLE__)
  if (not _mainWindow)
    return 0;
  auto ctxbase = dynamic_cast<CtxGLFW*>(_mainWindow->_ctqt);
  if (not ctxbase || not ctxbase->_glfwWindow)
    return 0;
  return nativeCocoaWindowNumber(ctxbase->_glfwWindow);
#else
  return 0; // BUG-B native leg is mac-only this slice; non-mac degrades to rect-only.
#endif
}

void OrkEzApp::closeAllSecondaryWindows() {
  for (auto& win : _secondaryWindows) {
    win->requestClose();
  }
}

void OrkEzApp::_renderSecondaryWindows() {
  // needsRender() is a dirty-flag + wall-clock staleness gate tuned for on-screen
  // windows kept warm by OS expose/refresh events. Under --offscreen or lockstep
  // there are NO such events, so the gate would starve secondary rendering non-
  // deterministically (a frame renders or not depending on wall-clock timing).
  // Force a render every pump in those modes so offscreen/lockstep captures are
  // reproducible (double-run byte-equal).
  const bool force_render = _initdata->_offscreen or (not _initdata->_freerunning);
  for (auto& win : _secondaryWindows) {
    if (!win->shouldClose() && (force_render or win->needsRender())) {
      win->_render();
    }
  }
}

void OrkEzApp::_cleanupClosedSecondaryWindows() {
  // Remove closed windows and return focus to main window if any were removed
  size_t before = _secondaryWindows.size();

  // Two-phase close: first call hides the window (phase 1),
  // second call destroys it (phase 2). This gives macOS a frame
  // to deliver the matching mouseUp before the window is destroyed,
  // preventing global click-blocking.
  for (auto& w : _secondaryWindows) {
    if (w->shouldClose()) {
      w->_forceClose();
    }
  }

  // Only erase windows whose GLFW window has been fully destroyed (phase 2 done)
  std::erase_if(_secondaryWindows, [](const auto& w) {
    return w->shouldClose() && w->_isFullyClosed();
  });

  size_t removed = before - _secondaryWindows.size();
  if (removed > 0) {
    logchan_ezapp->log("Removed %zu closed secondary window(s)", removed);
  }

  // Return focus to main window if any popups were closed
  if (_secondaryWindows.size() < before) {
    printf("_cleanupClosedSecondaryWindows: removed %zu window(s), restoring focus\n", removed);
#if defined(ENABLE_GLFW)
    if (_mainWindow && _mainWindow->_ctqt) {
      auto ctx = dynamic_cast<CtxGLFW*>(_mainWindow->_ctqt);
      if (ctx && ctx->_glfwWindow) {
        printf("  calling glfwFocusWindow(%p)\n", (void*)ctx->_glfwWindow);
        glfwFocusWindow(ctx->_glfwWindow);
      } else {
        printf("  FAILED: ctx=%p glfwWindow=%p\n", (void*)ctx, ctx ? (void*)ctx->_glfwWindow : nullptr);
      }
    } else {
      printf("  FAILED: _mainWindow=%p _ctqt=%p\n", (void*)_mainWindow.get(), _mainWindow ? (void*)_mainWindow->_ctqt : nullptr);
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
  // initModule auto-spawns the loader thread when it creates
  // gloadercontext. No explicit start here.
  init_data->finalizeInitialization();

  return ezapp;
}
