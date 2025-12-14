////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined(ORK_IOS)

#include <ork/ios/app_init.h>
#include <ork/application/application.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/file/path.h>
#include <ork/kernel/environment.h>
#include <ork/kernel/thread.h>
#include <ork/kernel/opq.h>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////
// iOS Core Application - manages StringPool lifecycle
///////////////////////////////////////////////////////////////////////////////
struct CoreIOSApplication {
  CoreIOSApplication() {
    _stringpoolctx = std::make_shared<ork::StringPoolContext>();
    StringPoolStack::push(_stringpoolctx);
  }
  ~CoreIOSApplication() {
    StringPoolStack::pop();
  }
  stringpoolctx_ptr_t _stringpoolctx;
};

///////////////////////////////////////////////////////////////////////////////
// Forward declarations from ork.core
///////////////////////////////////////////////////////////////////////////////
namespace ork {
  void initModule(ork::appinitdata_ptr_t init_data);
  void exitModule(ork::appinitdata_ptr_t init_data);
}

///////////////////////////////////////////////////////////////////////////////
// Global state
///////////////////////////////////////////////////////////////////////////////
static bool _core_initialized = false;

///////////////////////////////////////////////////////////////////////////////
// iOS Core Initialization (call once on main thread at startup)
///////////////////////////////////////////////////////////////////////////////
void _coreappinit(int argc, char** argv) {
  // Check if already initialized
  if (_core_initialized) {
    printf("WARNING: _coreappinit() called multiple times - ignoring\n");
    return;
  }
  _core_initialized = true;

  // Mark main thread
  SetCurrentThreadName("main");

  // Initialize environment from global env vars
  ork::genviron.init_from_global_env();

  // Create app init data
  gappinitdata = std::make_shared<AppInitData>(argc, argv);

  // Create core application (manages StringPool)
  static CoreIOSApplication the_app;

  // Setup file system context
  static auto WorkingDirContext = std::make_shared<FileDevContext>();
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());

  // Initialize Orkid core module
  ork::initModule(gappinitdata);

  printf("Orkid core initialized for iOS\n");
}

///////////////////////////////////////////////////////////////////////////////
// iOS Core Shutdown (call at app exit)
///////////////////////////////////////////////////////////////////////////////
void _coreappexit() {
  if (!_core_initialized) {
    printf("WARNING: _coreappexit() called without initialization - ignoring\n");
    return;
  }

  // Shutdown Orkid core module
  ork::exitModule(gappinitdata);
  gappinitdata = nullptr;
  _core_initialized = false;

  printf("Orkid core shutdown (iOS)\n");
}

///////////////////////////////////////////////////////////////////////////////
// iOS Core Poll (call every frame on main thread)
///////////////////////////////////////////////////////////////////////////////
void _coreapppoll() {
  // Process main serial queue operations
  while (ork::opq::mainSerialQueue()->Process()) {
    // Keep processing until queue is empty
  }
}

#endif // ORK_IOS
