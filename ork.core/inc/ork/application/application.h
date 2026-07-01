////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

//#include <ork/orkstl.h>

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/kernel/any.h>
#include <ork/util/Context.h>
#include <ork/file/file.h>

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/application/subsystem.h>
#include <ork/kernel/opq.h>
#include <ork/util/fsm.h>
#include <ork/util/crc.h>  // for CrcEnum macro + crc_enum_t (EFullScreenMode below)

#include <set>

#if !defined(ORK_IOS)
#include <boost/program_options.hpp>
#endif


namespace ork {

/**
 * Very high-level application code.
 */

struct AppInitData;

using appinitdata_ptr_t = std::shared_ptr<AppInitData>;
using appinitdata_wkptr_t = std::weak_ptr<AppInitData>;
using appinitfn_t = std::function<void(appinitdata_ptr_t)>;

// Global app init data - lazy singleton accessor (thread-safe)
// Returns the process-wide AppInitData instance, creating it on first access
appinitdata_ptr_t appinitdata();

#if !defined(ORK_IOS)
namespace po = ::boost::program_options;
#else
// Stub for iOS (no command line options)
namespace po {
  struct options_description {};
  struct variables_map {};
  struct variable_value {};
}
#endif

struct StdFileSystemInitalizer {
  StdFileSystemInitalizer(const AppInitData& initdata);
  ~StdFileSystemInitalizer();
  const AppInitData& _initdata;
};

using stdfilesysinit_p = std::shared_ptr<StdFileSystemInitalizer>;

enum class AppInitOrder : uint64_t {
  REFLECTION_CLASS_REGISTRATION = 0,
  REFLECTION_LINK = 100,
  GRAPHICS_INIT = 200
};

struct AppInitData{

  using opts_desc_t = po::options_description;
  using opts_desc_ptr_t = std::shared_ptr<po::options_description>;
  using opts_var_map_t = po::variables_map;
  using opts_var_map_ptr_t = std::shared_ptr<po::variables_map>;

  void enqueuePreInitOp(AppInitOrder order, void_lambda_t l);
  void enqueuePostInitOp(AppInitOrder order, void_lambda_t l);
  
  opts_desc_ptr_t commandLineOptions(const char* header_text);

  opts_var_map_ptr_t parse();
  const po::variable_value& commandLineOption(const std::string& named);

  AppInitData(int argc=0, char** argv=nullptr, char** envp = nullptr);
  ~AppInitData();

  void setArgs(int argc, char** argv, char** envp);


  void executePreInitOps();
  void executePostInitOps();
  void finalizeInitialization();

  int _argc = 0;
  char** _argv = nullptr;
  char** _envp = nullptr;

  std::shared_ptr<StdFileSystemInitalizer> _fsinit;

  std::map<std::string,svar64_t> _miscvars;

  opts_desc_ptr_t _commandline_desc;
  opts_var_map_ptr_t _commandline_vars;
  std::vector<std::string> _dynaargs_storage;
  std::vector<char*> _dynaargs_refs;

  bool _enable_audio = false;
  bool _enable_audio_input = false;
  bool _enable_audio_output = false;
  bool _enable_audio_synth = false;
  bool _enable_graphics = true;
  bool _std_asset_catalog = true;

  // Fullscreen presentation modes (CrcEnum for Python<->C++ string-keyed
  // pass-through; values hash via the CrcEnum macro defined in
  // ork/util/crc.h, so Python sets `fullscreen_mode="windowed"` or
  // `fullscreen_mode="immersive"` and the C++ side compares against
  // EFullScreenMode::Windowed / EFullScreenMode::Immersive).
  //   Immersive — covers the entire panel (under menu-bar / dock). The
  //               default; matches historical orkid behavior. Use for VR
  //               mirror windows, kiosk mode, video playback, game-style
  //               takeover, etc.
  //   Windowed  — borderless windowed at the workarea size (excludes
  //               OS menu bar / dock); framebuffer extends only to the
  //               bottom of the menu bar. Opt-in for apps that want to
  //               co-exist with the system UI on macOS.
  enum class EFullScreenMode : ::ork::crc_enum_t {
    CrcEnum(Windowed),
    CrcEnum(Immersive),
  };
  bool _fullscreen = false;
  EFullScreenMode _fullscreen_mode = EFullScreenMode::Immersive;
  bool _offscreen = false;
  bool _use_drm = false;  // Use DRM direct rendering (Linux only)
  bool _canalwaysontop = false;
  int _top = 100;
  int _left = 100;
  int _width = 1280;
  int _height = 720;
  int _msaa_samples = 0;   // MSAA LEVEL: 0=off, 1=2x, 2=4x, 3=8x, 4=16x (device-clamped). was 1 (=off under the old count semantics)
  int _ssaa_samples = 0;
  int _swap_interval = 0;
  bool _update_rendersync = false;
  bool _allowHIDPI = false;
  bool _disableMouseCursor = false;
  bool _fsMouseMode = false;  // Fullscreen mouse mode: hide HW cursor, render virtual
  std::string _audio_input_devname = "default";
  std::string _audio_output_devname = "default";
  std::string _audio_ioclass = "default";
  std::string _fullscreen_monitor = "none";
  std::string _drm_mode = "a0";  // DRM device + mode (e.g., "b0", "c2")
  size_t _audio_input_numchannels = 1;
  size_t _audio_output_numchannels = 2;
  bool _audio_stream_sync = false;
  bool _freerunning = true;
  float _target_ups = 480.0f;   // Updates per second (simulation tick rate)
  float _target_fps = 120.0f;   // Frames per second (render rate)
  bool _displaylink = false;       // Use CVDisplayLink/Metal swapchain for VR scanout prediction (fullscreen only)
  bool _log_freerun_ups = false;   // Enable real-time UPS logging in freerun mode
  bool _log_freerun_fps = false;   // Enable real-time FPS logging in freerun mode
  bool _log_lockstep_ups = false;  // Enable real-time UPS logging in lockstep mode
  bool _log_lockstep_fps = false;  // Enable real-time FPS logging in lockstep mode
  bool _use_subsystems = false;    // Enable HFSM subsystem lifecycle (Phase 2b)
  bool _defer_gpu_init = false;    // Defer GPU context creation until GPU subsystem init
  std::set<std::string> _enabled_subsystems;  // List-based subsystem selection (e.g., "gpu", "audioO")
  std::vector<subsystem_ptr_t> _custom_subsystems;  // Custom subsystems from Python
  std::string _monitor_id = "";
  std::string _application_name = "orkid_app";
  std::multimap<uint64_t,void_lambda_t> _preinitoperations;
  std::multimap<uint64_t,void_lambda_t> _postinitoperations;
  file::Path _movie_output_path;
  varmap::varmap_ptr_t _misc_varmap;
};

struct StringPoolContext {

	static PoolString AddPooledString(const PieceString &);
	static PoolString AddPooledLiteral(const ConstString &);
	static PoolString FindPooledString(const PieceString &);

	StringPoolContext();

private:

    StringPool _stringpool;

};

PoolString addPooledStringFromStdString(const std::string& str);
PoolString AddPooledString(const PieceString &ps);
PoolString AddPooledLiteral(const ConstString &cs);
PoolString FindPooledString(const PieceString &ps);

PoolString operator"" _pool(const char* s, size_t len);

////////////////////////////////////////////////////////////////
// Application - Base application class with HFSM lifecycle
//
// Provides:
// - Operation queues (mainq, updq, conq)
// - UPDATE thread management
// - Subsystem registry with dependency tracking
// - Dependency-driven init/shutdown
////////////////////////////////////////////////////////////////

struct Application;
using application_ptr_t = std::shared_ptr<Application>;

struct Application {
public:
  // Factory method - ensures finalize() is called after construction
  static application_ptr_t create();

  virtual ~Application();

  // Called by factory after construction to finalize initialization
  virtual void finalize();

  // Main run loop - processes queues, updates FSMs, calls user callback
  // Blocks until requestExit() is called or SIGINT received
  void mainThreadLoop(void_lambda_t on_iter = nullptr);

  // Request clean shutdown (can be called from any thread or signal handler)
  void requestExit();

  // Explicit shutdown - triggers subsystem shutdown in reverse dependency order
  // Call this before destruction to ensure clean subsystem teardown
  void shutdown();

  // Check if exit has been requested
  bool exitRequested() const { return _exit_requested.load(); }

  // Static accessor for signal handler
  static application_ptr_t instance() { return _g_application; }

  // Lifecycle (called by derived classes like OrkEzApp)
  void _initApp();
  void _shutdownApp();

  // Subsystem management (thread-safe, dynamic)
  // Dependencies must be set in subsystem->_dependencies before calling
  void registerSubsystem(
      subsystem_ptr_t subsystem,
      bool is_static = false);

  // Convenience overload for string names (auto-hashes to uint64_t)
  void registerSubsystem(
      const std::string& name,
      subsystem_ptr_t subsystem,
      bool is_static = false);

  void unregisterSubsystem(uint64_t name_hash);
  void unregisterSubsystem(const std::string& name);

  // Subsystem lookup
  subsystem_ptr_t getSubsystem(uint64_t name_hash) const;
  subsystem_ptr_t getSubsystem(const std::string& name) const;

  // Virtual lifecycle hooks (override in derived classes like OrkEzApp)
  // These are called at key points in the application lifecycle
  virtual void onAppInit() {}      // Called after core subsystems ready
  virtual void onAppUpdate() {}    // Called each main loop iteration
  virtual void onAppShutdown() {}  // Called before subsystem shutdown

  // Operation queues
  ork::opq::opq_ptr_t _mainq;  // Main/GPU thread queue
  ork::opq::opq_ptr_t _updq;   // UPDATE thread queue
  ork::opq::opq_ptr_t _conq;   // AUDIO thread queue

  // String pool for this application
  stringpoolctx_ptr_t _stringpoolctx;

  // App initialization data (global singleton)
  appinitdata_ptr_t _initdata;

protected:
  // Constructor - protected, use create() factory method
  Application();

  // Constructor for derived classes (e.g., OrkEzApp) that handle their own initialization
  // This constructor skips core module init since derived class will do it
  Application(appinitdata_ptr_t initdata, bool derived_class_init);

  // Singleton enforcement - only one Application per process
  static application_ptr_t _g_application;

  // Dependency-driven subsystem initialization
  void _initSubsystemsInWaves();
  std::vector<subsystem_reg_ptr_t> _getReadyToInitSubsystems();

  // Dependency-driven subsystem shutdown (reverse order)
  void _shutdownSubsystemsInWaves();
  void _buildShutdownWaves(std::vector<std::vector<subsystem_reg_ptr_t>>& waves);

  // Subsystem registry (thread-safe)
  mutable std::mutex _subsystem_mutex;
  std::unordered_map<uint64_t, subsystem_reg_ptr_t> _registered_subsystems;

  // UPDATE thread
  ork::Thread _update_thread;
  std::atomic<bool> _update_thread_running{false};

  // Exit flag (set by requestExit() or signal handler)
  std::atomic<bool> _exit_requested{false};

  // Shutdown state (for idempotent shutdown())
  std::atomic<bool> _shutdown_complete{false};
};

}

using stringpoolctx_ptr_t = std::shared_ptr<ork::StringPoolContext>;
using StringPoolStack = ork::util::GlobalStack<stringpoolctx_ptr_t>;
