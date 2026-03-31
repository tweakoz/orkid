////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <iostream>
#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/application/subsystem_opq.h>
#include <ork/application/subsystem_catalog.h>
#include <ork/application/subsystem_core.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/PoolString.h>

#include <ork/util/Context.hpp>
#include <ork/kernel/environment.h>
#include <ork/file/path.h>
#include <ork/util/logger.h>
#include <ork/kernel/future.hpp>
#include <thread>
#include <chrono>
#include <csignal>

int desired_framesize = 1024; // audio framesize from environment or command line
///////////////////////////////////////////////////////////////////////////////
namespace ork {

// Forward declarations for core module init/exit
void initModule(ork::appinitdata_ptr_t init_data);
void exitModule(ork::appinitdata_ptr_t init_data);

static logchannel_ptr_t logchan_APP = logger()->configureChannel("APPLICATION", fvec3(0.9, 0.6, 0.2), true);

// Global application init data - lazy singleton (thread-safe via C++11 static initialization)
appinitdata_ptr_t appinitdata() {
  static appinitdata_ptr_t _g_appinitdata = std::make_shared<AppInitData>();
  return _g_appinitdata;
}
///////////////////////////////////////////////////////////////////////////////
AppInitData::AppInitData(int argc, char** argv, char** envp) {
  setArgs(argc, argv, envp);
}

///////////////////////////////////////////////////////////////////////////////

void AppInitData::setArgs(int argc, char** argv, char** envp) {
  _argc             = argc;
  _argv             = argv;
  _envp             = envp;
  _commandline_vars = std::make_shared<opts_var_map_t>();
  _fsinit           = std::make_shared<StdFileSystemInitalizer>(*this);
  _audio_ioclass    = "default";
  _misc_varmap      = std::make_shared<varmap::VarMap>();

  if (genviron.has("ORKID_AUDIO_INPUT_DEVICE")) {
    std::string audioinputdev;
    genviron.get("ORKID_AUDIO_INPUT_DEVICE", audioinputdev);
    _audio_input_devname = audioinputdev;
  }
  if (genviron.has("ORKID_AUDIO_OUTPUT_DEVICE")) {
    std::string audiooutputdev;
    genviron.get("ORKID_AUDIO_OUTPUT_DEVICE", audiooutputdev);
    _audio_output_devname = audiooutputdev;
  }
  if (genviron.has("ORKID_AUDIO_IOCLASS")) {
    std::string audioioclass;
    genviron.get("ORKID_AUDIO_IOCLASS", audioioclass);
    _audio_ioclass = audioioclass;
  }
  if (genviron.has("ORKID_AUDIO_STREAM_SYNC")) {
    std::string sync_enable_str;
    genviron.get("ORKID_AUDIO_STREAM_SYNC", sync_enable_str);
    _audio_stream_sync = (sync_enable_str == "1") || (sync_enable_str == "true");
  }
  if (genviron.has("ORKID_DISABLE_ALWAYS_ON_TOP")) {
    _canalwaysontop = false;
  }
  if (genviron.has("ORKID_AUDIO_FRAMESIZE")) {
    std::string framesize_str;
    genviron.get("ORKID_AUDIO_FRAMESIZE", framesize_str);
    desired_framesize = atoi(framesize_str.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::~AppInitData() {
}

///////////////////////////////////////////////////////////////////////////////

void AppInitData::enqueuePreInitOp(AppInitOrder order, void_lambda_t l) { //
  _preinitoperations.insert(std::pair(uint64_t(order), l));
}

///////////////////////////////////////////////////////////////////////////////

void AppInitData::enqueuePostInitOp(AppInitOrder order, void_lambda_t l) { //
  _postinitoperations.insert(std::pair(uint64_t(order), l));
}

void AppInitData::executePreInitOps() {
  if(0)logchan_APP->log("AppInitData::executePreInitOps");
  for (auto item : _preinitoperations) {
    uint64_t order = item.first;
    auto operation = item.second;
    operation();
  }
}
void AppInitData::executePostInitOps() {
  if(0)logchan_APP->log("AppInitData::executePostInitOps");
  for (auto item : _postinitoperations) {
    uint64_t order = item.first;
    auto operation = item.second;
    operation();
  }
}

///////////////////////////////////////////////////////////////////////////////

void AppInitData::finalizeInitialization() {
  executePreInitOps();
  executePostInitOps();
  _preinitoperations.clear();
  _postinitoperations.clear();
  // NOTE: Catalog initialization is now handled by the CATALOG subsystem
  // in Application::Application() via _initSubsystemsInWaves()
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::opts_desc_ptr_t AppInitData::commandLineOptions(const char* header_text) {
#if !defined(ORK_IOS)
  _commandline_desc = std::make_shared<opts_desc_t>(header_text);
  _commandline_vars = std::make_shared<opts_var_map_t>();
  return _commandline_desc;
#else
  return nullptr;
#endif
}

///////////////////////////////////////////////////////////////////////////////

const po::variable_value& AppInitData::commandLineOption(const std::string& named) {
#if !defined(ORK_IOS)
  return (*_commandline_vars)[named];
#else
  static po::variable_value stub;
  return stub;
#endif
}

///////////////////////////////////////////////////////////////////////////////

AppInitData::opts_var_map_ptr_t AppInitData::parse() {
#if !defined(ORK_IOS)
  if (_commandline_desc) {
    auto cmdline = po::parse_command_line(_argc, _argv, *_commandline_desc);
    po::store(cmdline, *_commandline_vars);
    po::notify(*_commandline_vars);
  }
  if (_commandline_vars->count("help")) {
    std::cout << (*_commandline_desc) << "\n";
    exit(0);
  }
  auto& vars = *_commandline_vars;

  if (_commandline_vars->count("fullscreen")) {
    this->_fullscreen = vars["fullscreen"].as<bool>();
  }
  if (_commandline_vars->count("offscreen")) {
    this->_offscreen  = vars["offscreen"].as<bool>();
    this->_fullscreen = false;
  }
#if defined(__linux__)
  // DRM mode (Linux only)
  if (_commandline_vars->count("drm-list")) {
    if (vars["drm-list"].as<bool>()) {
      this->_miscvars["drm-list"].set<bool>(true);
    }
  }
  if (_commandline_vars->count("drm")) {
    this->_use_drm    = true;
    this->_drm_mode   = vars["drm"].as<std::string>();
    this->_fullscreen = false;
    this->_offscreen  = false;
  }
#endif
  if (_commandline_vars->count("enable_audio")) {
    this->_enable_audio = vars["enable_audio"].as<bool>();
  }
  if (_commandline_vars->count("top")) {
    this->_top = vars["top"].as<int>();
  }
  if (_commandline_vars->count("left")) {
    this->_left = vars["left"].as<int>();
  }
  if (_commandline_vars->count("width")) {
    this->_width = vars["width"].as<int>();
  }
  if (_commandline_vars->count("height")) {
    this->_height = vars["height"].as<int>();
  }
  if (_commandline_vars->count("msaa")) {
    this->_msaa_samples = vars["msaa"].as<int>();
  }
  if (_commandline_vars->count("ssaa")) {
    this->_ssaa_samples = vars["ssaa"].as<int>();
  }
  // https://download.nvidia.com/XFree86/Linux-x86_64/525.78.0/README/openglenvvariables.html
  if (_commandline_vars->count("nvsync")) {

    bool do_vsync = vars["nvsync"].as<bool>();
    genviron.set("__GL_SYNC_TO_VBLANK", do_vsync ? "1" : "0");
    if (do_vsync) {
      int vsport = vars["nvsport"].as<int>();
      genviron.set("__GL_SYNC_DISPLAY_DEVICE", FormatString("DFP-%d", vsport));
    }
  }
  if (_commandline_vars->count("nvmfa")) {
    int vmfa = vars["nvmfa"].as<int>();
    genviron.set("__GL_MaxFramesAllowed", FormatString("%d", vmfa));
  }

  // genviron.dump();

  // printf("_msaa_samples<%d>\n", this->_msaa_samples);
  return _commandline_vars;
#else
  // iOS: no command line parsing
  return nullptr;
#endif
}

StdFileSystemInitalizer::StdFileSystemInitalizer(const AppInitData& appinitdata)
    : _initdata(appinitdata) {
  // printf("CPA\n");
  //  OldSchool::SetGlobalStringVariable("temp://", CreateFormattedString("ork.data/temp/"));

  // printf("CPB\n");
  //////////////////////////////////////////
  // Register data:// urlbase

  // todo - hold somewhere not static
  static auto WorkingDirContext = std::make_shared<FileDevContext>();

  auto base_dir  = file::Path::orkroot_dir();
  auto stage_dir = file::Path::stage_dir();
  auto src_core  = base_dir / "ork.core";
  auto src_lev2  = base_dir / "ork.lev2";
  auto data_dir  = base_dir / "ork.data";
  auto lev2_base = data_dir / "platform_lev2";
  auto srcd_base = data_dir / "src";

  //////////////////////////////////////////
  // Register path expanders (token-only, no URI protocol needed)
  // These handle <assetcache> and <staging> tokens in expandPaths()
  //////////////////////////////////////////

  file::setPathExpander("assetcache",   stage_dir / "assetcache");
  file::setPathExpander("staging",      stage_dir);
  file::setPathExpander("ork_ecsscenes", data_dir / "ecsscenes");
  file::setPathExpander("ork_envmaps", stage_dir / "envmaps");
  file::setPathExpander("ork_data",     data_dir);
  file::setPathExpander("ork_testdata", data_dir / "tests");

  //////////////////////////////////////////
  // Register urlbases (also populates expander table via bidirectional sync)
  //////////////////////////////////////////

  auto LocPlatformLevel2FileContext   = FileEnv::createContextForUriBase("lev2://", lev2_base);
  auto SrcPlatformLevel2FileContext   = FileEnv::createContextForUriBase("src://", srcd_base);
  auto LocPlatformMorkDataFileContext = FileEnv::createContextForUriBase("miniorkdata://", srcd_base);
  auto DataDirContext                 = FileEnv::createContextForUriBase("data://", data_dir);
  auto OrkidDirContext                = FileEnv::createContextForUriBase("orkid://", base_dir);
  auto OrkidCoreContext               = FileEnv::createContextForUriBase("ork_core://", src_core);
  auto OrkidLev2Context               = FileEnv::createContextForUriBase("ork_lev2://", src_lev2);

  //////////////
  // we dont want to see lev2:// in choice manager
  //////////////
  LocPlatformLevel2FileContext->_vars.makeValueForKey<void*>("disablechoices");
  //////////////
}
StdFileSystemInitalizer::~StdFileSystemInitalizer() {
}

// po::options_description desc("Allowed options");
// desc.add_options()                                                //
//   ("help", "produce help message")                              //
//("test", po::value<std::string>(), "test name (list,vo,nvo)") //
//("port", po::value<std::string>(), "midiport name (list)")    //
//("program", po::value<std::string>(), "program name")         //
//("hidpi", "hidpi mode");

// po::variables_map vars;
// po::store(po::parse_command_line(argc, argv, desc), vars);
// po::notify(vars);

PoolString addPooledStringFromStdString(const std::string& str) {
  return StringPoolContext::AddPooledString(PieceString(str.c_str()));
}
PoolString AddPooledString(const PieceString& ps) {
  return StringPoolContext::AddPooledString(ps);
}
PoolString AddPooledLiteral(const ConstString& cs) {
  return StringPoolContext::AddPooledLiteral(cs);
}
PoolString FindPooledString(const PieceString& ps) {
  return StringPoolContext::FindPooledString(ps);
}

StringPoolContext::StringPoolContext() {
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::AddPooledString(const PieceString& string) {
  PoolString result = FindPooledString(string);
  if (result)
    return result;

  ResizableString copy(string);
  const char* data = copy.c_str();
  new (&copy) ResizableString;

  auto papp = StringPoolStack::top();
  OrkAssert(papp);

  return papp->_stringpool.String(data);
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::AddPooledLiteral(const ConstString& string) {
  PoolString result = FindPooledString(string);
  if (result)
    return result;

  auto app = StringPoolStack::top();
  OrkAssert(app);

  return app->_stringpool.Literal(string);
}

///////////////////////////////////////////////////////////////////////////////

PoolString StringPoolContext::FindPooledString(const PieceString& string) {

  auto pAPP         = StringPoolStack::top();
  PoolString result = pAPP->_stringpool.Find(string);
  if (result)
    return result;
  return PoolString();
}

///////////////////////////////////////////////////////////////////////////////

PoolString operator"" _pool(const char* s, size_t len) {
  return AddPooledString(s);
}

///////////////////////////////////////////////////////////////////////////////
// Application class implementation
///////////////////////////////////////////////////////////////////////////////

// Singleton enforcement
application_ptr_t Application::_g_application = nullptr;

///////////////////////////////////////////////////////////////////////////////

application_ptr_t Application::create() {
  OrkAssert(_g_application == nullptr && "Only one Application allowed per process");
  auto app = application_ptr_t(new Application());
  app->finalize();
  _g_application = app;
  return app;
}

///////////////////////////////////////////////////////////////////////////////

void Application::finalize() {
  // Called after construction complete - execute post-init ops
  _initdata->finalizeInitialization();
}

///////////////////////////////////////////////////////////////////////////////

Application::Application() {
  // Singleton guard
  OrkAssert(_g_application == nullptr && "Only one Application allowed per process");

  // Set main thread name
  SetCurrentThreadName("main");

  // Initialize environment from global env vars
  ork::genviron.init_from_global_env();

  // Get global singleton AppInitData (required by rest of codebase)
  _initdata = ::ork::appinitdata();

  // Set data:// path to orkroot
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());

  // Initialize core module - registers reflection classes and creates global OPQs
  ork::initModule(_initdata);

  // Create and register OPQ subsystem (wraps global OPQ lifecycle)
  auto opq_subsystem = createOpqSubsystem();
  registerSubsystem(opq_subsystem, true);  // Mark as static subsystem

  // Initialize OPQ subsystem immediately (needed for queue references below)
  opq_subsystem->initialize();
  opq_subsystem->update();  // Process state transitions

  // Get references to global OPQs created by opq::init()
  _mainq = opq::mainSerialQueue();
  _updq = opq::updateSerialQueue();
  _conq = opq::concurrentQueue();

  // Create CATALOG subsystem (optional, part of core layer)
  subsystem_ptr_t catalog_subsystem = nullptr;
  if (_initdata->_std_asset_catalog) {
    catalog_subsystem = createCatalogSubsystem();
    catalog_subsystem->addDependency(opq_subsystem);  // CATALOG depends on OPQ
    registerSubsystem(catalog_subsystem, true);
  }

  // Create CORE subsystem (meta-service: signals "all core services ready/down")
  auto core_subsystem = createCoreSubsystem();
  core_subsystem->addDependency(opq_subsystem);  // CORE depends on OPQ
  if (catalog_subsystem) {
    core_subsystem->addDependency(catalog_subsystem);  // CORE depends on CATALOG
  }
  registerSubsystem(core_subsystem, true);

  // Initialize remaining static subsystems in dependency order
  _initSubsystemsInWaves();

  // Create string pool context
  _stringpoolctx = std::make_shared<StringPoolContext>();

  // Call virtual hook for derived class initialization
  onAppInit();
}

///////////////////////////////////////////////////////////////////////////////
// Constructor for derived classes (e.g., OrkEzApp) that handle their own initialization
// This constructor skips core module init and subsystem creation since derived class does it
///////////////////////////////////////////////////////////////////////////////

Application::Application(appinitdata_ptr_t initdata, bool derived_class_init) {
  // Singleton guard - allow derived class to be the singleton
  OrkAssert(_g_application == nullptr && "Only one Application allowed per process");

  // Store initdata from derived class
  _initdata = initdata;

  // Get references to global OPQs (assumed already initialized by derived class)
  _mainq = opq::mainSerialQueue();
  _updq = opq::updateSerialQueue();
  _conq = opq::concurrentQueue();

  // NOTE: Derived class is responsible for:
  // - SetCurrentThreadName("main")
  // - Environment initialization
  // - Core/lev2 module initialization
  // - String pool context creation (if needed)
  // - Subsystem registration (if needed)

  logchan_APP->log("Application(derived_class_init) constructed");
}

///////////////////////////////////////////////////////////////////////////////

Application::~Application() {
  logchan_APP->log("Application destructor");

  // Call shutdown() if not already called (idempotent)
  shutdown();

  logchan_APP->log("Application destructor complete");

  // Clear singleton
  _g_application = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void Application::shutdown() {
  // Idempotent - only run once
  bool expected = false;
  if (!_shutdown_complete.compare_exchange_strong(expected, true)) {
    logchan_APP->log("Application::shutdown() - already complete, skipping");
    return;
  }

  logchan_APP->log("Application::shutdown() - beginning subsystem teardown");

  // Call virtual hook for derived class cleanup (before subsystem shutdown)
  try {
    onAppShutdown();
  } catch (const std::exception& e) {
    logchan_APP->log("Exception in onAppShutdown: %s", e.what());
  } catch (...) {
    logchan_APP->log("Unknown exception in onAppShutdown");
  }

  // Shutdown all registered subsystems (in reverse dependency order)
  // This includes OPQ subsystem which drains queues before cleanup
  _shutdownSubsystemsInWaves();

  // NOTE: We deliberately do NOT call exitModule() here
  // The OPQ subsystem already drained the queues in its shutdown handler
  // Global OPQs will be cleaned up at process exit (no regression from old behavior)
  // This avoids the mutex crash in opq::exit()

  logchan_APP->log("Application::shutdown() complete");
}

///////////////////////////////////////////////////////////////////////////////

void Application::_initApp() {
  // Placeholder - will be filled in next tasks
  logchan_APP->log("Application::_initApp()");
}

///////////////////////////////////////////////////////////////////////////////

void Application::_shutdownApp() {
  // Placeholder - will be filled in next tasks
  logchan_APP->log("Application::_shutdownApp()");
}

///////////////////////////////////////////////////////////////////////////////
// Signal handler for SIGINT (Ctrl-C)
// Uses the global singleton to request exit
///////////////////////////////////////////////////////////////////////////////

static void _signalHandler(int signum) {
  if (signum == SIGINT) {
    auto app = Application::instance();
    if (app) {
      app->requestExit();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Application::requestExit() {
  bool expected = false;
  if (_exit_requested.compare_exchange_strong(expected, true)) {
    logchan_APP->log("Exit requested");
  }
}

///////////////////////////////////////////////////////////////////////////////

void Application::mainThreadLoop(void_lambda_t on_iter) {
  logchan_APP->log("Application::mainThreadLoop() starting");

  // Install signal handler for clean shutdown on Ctrl-C
  std::signal(SIGINT, _signalHandler);

  // Main run loop
  while (!_exit_requested.load()) {
    // 1. Process main queue operations
    while (_mainq->Process()) {
      // Keep processing until queue is empty
    }

    // 2. Update all subsystem FSMs
    {
      std::lock_guard<std::mutex> lock(_subsystem_mutex);
      for (auto& [hash, reg] : _registered_subsystems) {
        if (reg->subsystem) {
          reg->subsystem->update();
        }
      }
    }

    // 3. Call virtual update hook (for derived classes)
    try {
      onAppUpdate();
    } catch (const std::exception& e) {
      logchan_APP->log("Exception in onAppUpdate: %s", e.what());
    } catch (...) {
      logchan_APP->log("Unknown exception in onAppUpdate");
    }

    // 4. Call user callback (if provided)
    if (on_iter) {
      try {
        on_iter();
      } catch (const std::exception& e) {
        logchan_APP->log("Exception in mainThreadLoop callback: %s", e.what());
      } catch (...) {
        logchan_APP->log("Unknown exception in mainThreadLoop callback");
      }
    }

    // 5. Small sleep to avoid spinning CPU
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  logchan_APP->log("Application::mainThreadLoop() exiting - shutdown requested");

  // Restore default signal handler
  std::signal(SIGINT, SIG_DFL);
}

///////////////////////////////////////////////////////////////////////////////

void Application::registerSubsystem(
    subsystem_ptr_t subsystem,
    bool is_static) {

  OrkAssert(subsystem);

  std::lock_guard<std::mutex> lock(_subsystem_mutex);

  uint64_t hash = subsystem->_name_hash;

  // Check if already registered
  if (_registered_subsystems.count(hash)) {
    logchan_APP->log("ERROR: Subsystem '%s' already registered", subsystem->_name.c_str());
    return;
  }

  // Create registration record
  auto reg = std::make_shared<SubsystemRegistration>();
  reg->subsystem = subsystem;
  reg->name_hash = hash;
  reg->is_static = is_static;

  _registered_subsystems[hash] = reg;

  if(0)logchan_APP->log("Registered subsystem '%s' (hash: 0x%016llx, static: %d)",
                   subsystem->_name.c_str(), hash, is_static);
}

///////////////////////////////////////////////////////////////////////////////

void Application::registerSubsystem(
    const std::string& name,
    subsystem_ptr_t subsystem,
    bool is_static) {

  // Convenience overload - just calls the primary version
  registerSubsystem(subsystem, is_static);
}

///////////////////////////////////////////////////////////////////////////////

void Application::unregisterSubsystem(uint64_t name_hash) {
  std::lock_guard<std::mutex> lock(_subsystem_mutex);

  auto it = _registered_subsystems.find(name_hash);
  if (it != _registered_subsystems.end()) {
    if(0)logchan_APP->log("Unregistered subsystem '%s'", it->second->subsystem->_name.c_str());
    _registered_subsystems.erase(it);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Application::unregisterSubsystem(const std::string& name) {
  uint64_t hash = CrcString(name.c_str()).hashed();
  unregisterSubsystem(hash);
}

///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t Application::getSubsystem(uint64_t name_hash) const {
  std::lock_guard<std::mutex> lock(_subsystem_mutex);

  auto it = _registered_subsystems.find(name_hash);
  if (it != _registered_subsystems.end()) {
    return it->second->subsystem;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t Application::getSubsystem(const std::string& name) const {
  uint64_t hash = CrcString(name.c_str()).hashed();
  return getSubsystem(hash);
}

///////////////////////////////////////////////////////////////////////////////

void Application::_initSubsystemsInWaves() {
  logchan_APP->log("Initializing subsystems (dependency-driven)");

  // Keep processing waves until all subsystems are initialized
  while (true) {
    auto ready_subsystems = _getReadyToInitSubsystems();

    if (ready_subsystems.empty()) {
      break;  // All done
    }

    logchan_APP->log("Initializing %zu subsystems (wave)", ready_subsystems.size());

    // Initialize this wave in parallel using ork::Future
    std::vector<std::shared_ptr<Future>> futures;
    std::vector<std::thread> threads;

    for (auto& reg : ready_subsystems) {
      reg->is_initializing = true;

      auto fut = std::make_shared<Future>();
      fut->_name = FormatString("init_%s", reg->subsystem->_name.c_str());
      futures.push_back(fut);

      threads.emplace_back([reg, fut]() {
        // Send START event to subsystem FSM
        reg->subsystem->_instance->sendEvent("START");

        // Process until READY, ERROR, or forced shutdown (SHUTTING_DOWN/TERMINATED)
        while (true) {
          fsm::FsmInstance::update(reg->subsystem->_instance);

          auto state = reg->subsystem->currentState();
          if (state == reg->subsystem->_state_ready ||
              state == reg->subsystem->_state_error ||
              state == reg->subsystem->_state_shutting_down ||
              state == reg->subsystem->_state_terminated) {
            break;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Signal completion
        fut->signal<bool>(true);
      });
    }

    // Wait for wave to complete
    for (size_t i = 0; i < futures.size(); i++) {
      auto& fut = futures[i];
      //logchan_APP->log("  waiting for future<%s>...", fut->_name.c_str());
      fut->waitForSignal();
      //logchan_APP->log("  future<%s> signaled", fut->_name.c_str());
    }

    // Join all threads
    for (auto& t : threads) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  logchan_APP->log("All subsystems initialized");
}

///////////////////////////////////////////////////////////////////////////////

std::vector<subsystem_reg_ptr_t> Application::_getReadyToInitSubsystems() {
  std::lock_guard<std::mutex> lock(_subsystem_mutex);

  std::vector<subsystem_reg_ptr_t> ready;

  for (auto& [name_hash, reg] : _registered_subsystems) {
    // Skip if already initializing or initialized
    if (reg->is_initializing) continue;
    auto current_state = reg->subsystem->currentState();
    if (current_state == reg->subsystem->_state_ready) continue;

    // Check if all dependencies are ready
    bool all_deps_ready = true;
    for (auto& [dep_hash, dep_subsystem] : reg->subsystem->_dependencies) {
      auto dep_state = dep_subsystem->currentState();
      if (dep_state != dep_subsystem->_state_ready) {
        all_deps_ready = false;
        break;
      }
    }

    if (!all_deps_ready) {
      continue;
    }

    // Check if all children are ready (children must init before parent)
    bool all_children_ready = true;
    for (auto& [child_hash, child_subsystem] : reg->subsystem->_children) {
      auto child_state = child_subsystem->currentState();
      if (child_state != child_subsystem->_state_ready) {
        all_children_ready = false;
        break;
      }
    }

    if (all_children_ready) {
      ready.push_back(reg);
    }
  }

  return ready;
}

///////////////////////////////////////////////////////////////////////////////

void Application::_shutdownSubsystemsInWaves() {
  //logchan_APP->log("Shutting down subsystems (dependency-driven reverse order)");

  // Build shutdown waves (reverse of init order)
  std::vector<std::vector<subsystem_reg_ptr_t>> shutdown_waves;
  _buildShutdownWaves(shutdown_waves);

  // Process waves in order (each wave shuts down in parallel)
  int wave_index = 0;
  for (auto& wave : shutdown_waves) {
    logchan_APP->log("Shutting down %zu subsystems (wave %d)", wave.size(), wave_index);

    std::vector<std::shared_ptr<Future>> futures;
    std::vector<std::thread> threads;

    for (auto& reg : wave) {
      reg->is_shutting_down = true;

      auto fut = std::make_shared<Future>();
      fut->_name = FormatString("shutdown_%s", reg->subsystem->_name.c_str());
      futures.push_back(fut);

      if(0)logchan_APP->log("  launching shutdown thread for subsystem<%s>", reg->subsystem->_name.c_str());

      threads.emplace_back([reg, fut]() {
        // Send SHUTDOWN event to subsystem FSM
        reg->subsystem->_instance->sendEvent("SHUTDOWN");

        // Process until TERMINATED
        while (true) {
          fsm::FsmInstance::update(reg->subsystem->_instance);

          auto state = reg->subsystem->currentState();
          if (state == reg->subsystem->_state_terminated) {
            break;
          }

          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Signal completion
        fut->signal<bool>(true);
      });
    }

    // Wait for wave to complete
    for (size_t i = 0; i < futures.size(); i++) {
      auto& fut = futures[i];
      //logchan_APP->log("  waiting for future<%s>...", fut->_name.c_str());
      fut->waitForSignal();
      //logchan_APP->log("  future<%s> signaled", fut->_name.c_str());
    }

    // Join all threads
    for (auto& t : threads) {
      if (t.joinable()) {
        t.join();
      }
    }

    wave_index++;
  }

  logchan_APP->log("All subsystems terminated");
}

///////////////////////////////////////////////////////////////////////////////

void Application::_buildShutdownWaves(std::vector<std::vector<subsystem_reg_ptr_t>>& waves) {
  std::lock_guard<std::mutex> lock(_subsystem_mutex);

  std::set<uint64_t> shutdown_scheduled;
  std::map<uint64_t, subsystem_reg_ptr_t> root_subsystems;

  // Only include ROOT subsystems (those with no parent)
  // Children are shutdown by their parent's shutdownChildren() call
  for (auto& [hash, reg] : _registered_subsystems) {
    if (!reg->subsystem->hasParent()) {
      root_subsystems[hash] = reg;
    } else {
      if(0)logchan_APP->log("  %s is a child (parent: %s), will be shutdown by parent",
                       reg->subsystem->_name.c_str(),
                       reg->subsystem->parent()->_name.c_str());
    }
  }

  if(0)logchan_APP->log("Building shutdown waves for %zu root subsystems", root_subsystems.size());

  // Build waves by reverse topological sort (dependents first, dependencies last)
  // A root subsystem can be scheduled when:
  // 1. No other (unscheduled) ROOT subsystem depends on it
  while (shutdown_scheduled.size() < root_subsystems.size()) {
    std::vector<subsystem_reg_ptr_t> wave;

    for (auto& [hash, reg] : root_subsystems) {
      if (shutdown_scheduled.count(hash)) continue;  // Already scheduled

      // Check: No other non-scheduled ROOT subsystem depends on this one
      bool has_dependents = false;
      for (auto& [other_hash, other_reg] : root_subsystems) {
        if (shutdown_scheduled.count(other_hash)) continue;
        if (other_hash == hash) continue;

        // Does other_reg depend on this one?
        if (other_reg->subsystem->_dependencies.count(hash)) {
          has_dependents = true;
          break;
        }
      }

      if (has_dependents) {
        continue;  // Can't schedule yet - another root still depends on us
      }

      // Check passed - this root subsystem can be scheduled
      wave.push_back(reg);
    }

    if (wave.empty() && shutdown_scheduled.size() < root_subsystems.size()) {
      logchan_APP->log("ERROR: Circular dependency detected in root subsystems during shutdown");
      // Force shutdown remaining root subsystems
      for (auto& [hash, reg] : root_subsystems) {
        if (!shutdown_scheduled.count(hash)) {
          wave.push_back(reg);
        }
      }
    }

    waves.push_back(wave);

    for (auto& reg : wave) {
      shutdown_scheduled.insert(reg->name_hash);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template struct ork::util::GlobalStack<stringpoolctx_ptr_t>;
