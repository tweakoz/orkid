# HFSM Application Lifecycle: Core Layering Architecture

## Executive Summary

This document describes how to properly layer the HFSM-based application lifecycle across `ork.core` and `ork.lev2` to support both:
- **Server-side applications** (core only, no graphics)
- **GUI applications** (lev2, with graphics/audio/UI)

**Key Principle:** Basic lifecycle (queues, threads, subsystems, FSM) in `ork.core`, extended by `ork.lev2` for GPU/Audio/UI.

**Related Documents:**
- **[apphfsm.md](./apphfsm.md)** - Complete HFSM application lifecycle design

---

## Current vs. Proposed Architecture

### Current (apphfsm.md)

| Component | Location | Issue |
|-----------|----------|-------|
| AppFsm | lev2 | ❌ Requires graphics layer |
| Subsystem coordination | lev2 | ❌ Can't use for servers |
| OPQ management | lev2/EzApp | ❌ Coupled to GUI |
| UPDATE thread | lev2/EzApp | ❌ Coupled to rendering |

**Problem:** Server-side apps (using ork.core only) have no standard lifecycle framework.

### Proposed (Layered)

| Component | Location | Benefit |
|-----------|----------|---------|
| **Application** class | ork.core | ✅ Server apps can use |
| AppFsm (core states) | ork.core | ✅ No graphics dependencies |
| Subsystem coordination | ork.core | ✅ Reusable everywhere |
| OPQ management | ork.core/Application | ✅ Standard queue lifecycle |
| UPDATE thread | ork.core/Application | ✅ Decoupled from rendering |
| **OrkEzApp** extends Application | ork.lev2 | ✅ Adds GPU/Audio/UI |
| GPU/Audio subsystems | ork.lev2 | ✅ Optional, not required |

---

## Design: ork.core::Application Base Class

### File Structure

```
ork.core/inc/ork/application/
├── application.h        (NEW: Application base class with FSM lifecycle)
├── appfsm.h            (NEW: Core AppFsm without GPU/Audio states)
├── subsystem.h         (NEW: SubsystemFsm base class)
└── (existing files unchanged)

ork.core/src/application/
├── application.cpp      (NEW: Core lifecycle implementation)
├── appfsm.cpp          (NEW: Build core FSM states)
├── subsystem.cpp       (NEW: Subsystem coordination)
└── application.cpp     (EXISTING: AppInitData, unchanged)
```

### Application Class Interface

```cpp
// ork.core/inc/ork/application/application.h

namespace ork {

// Forward declarations
class Application;
class SubsystemFsm;
struct SubsystemRegistration;

using application_ptr_t = std::shared_ptr<Application>;
using subsystemfsm_ptr_t = std::shared_ptr<SubsystemFsm>;
using subsystem_reg_ptr_t = std::shared_ptr<SubsystemRegistration>;

///////////////////////////////////////////////////////////////////////////////
// Application - Base class for all Orkid applications
// Provides FSM-based lifecycle without graphics dependencies
// Uses static factory pattern (no shared_from_this)
///////////////////////////////////////////////////////////////////////////////

class Application {
public:
    // Static factory pattern (returns shared_ptr)
    static application_ptr_t create(appinitdata_ptr_t initdata);

    virtual ~Application();

    // Main entry point
    virtual void mainThreadLoop();

    // Subsystem management (thread-safe, dynamic)
    // Dependencies must be set in subsystem->_dependencies before calling
    void registerSubsystem(
        subsystemfsm_ptr_t subsystem,
        bool is_static = false
    );

    // Convenience overload for string names (auto-hashes to uint64_t)
    void registerSubsystem(
        const std::string& name,
        subsystemfsm_ptr_t subsystem,
        bool is_static = false
    );

    void unregisterSubsystem(const std::string& name);
    subsystemfsm_ptr_t findSubsystem(const std::string& name) const;
    bool isSubsystemRunning(const std::string& name) const;

    // Query
    size_t subsystemCount() const;
    std::vector<std::string> subsystemNames() const;

    // Shutdown
    virtual void signalExit();

    // Access
    fsm::fsminstance_ptr_t getFsm() const { return _app_fsm; }
    opq::opq_ptr_t getMainQueue() const { return _mainq; }
    opq::opq_ptr_t getUpdateQueue() const { return _updq; }
    opq::opq_ptr_t getConcurrentQueue() const { return _conq; }

protected:
    // Constructor (protected - use factory)
    Application(appinitdata_ptr_t initdata);

    // === VIRTUAL HOOKS FOR DERIVED CLASSES ===

    // Build FSM - override to add states (call base first!)
    virtual void buildApplicationFsm();

    // Lifecycle extension points (called from FSM state callbacks)
    virtual void onAppInitExtension() {}
    virtual void onUpdateExtension() {}
    virtual void onShutdownExtension() {}

    // Subsystem helpers (derived classes can override)
    virtual void initStaticSubsystems();
    virtual void shutdownStaticSubsystems();

    // === PROTECTED MEMBERS (accessible to derived classes) ===

    appinitdata_ptr_t _initdata;

    // FSM
    fsm::fsminstance_ptr_t _app_fsm;
    fsm::fsmdata_ptr_t _fsm_data;

    // Core states (cached references)
    fsm::state_ptr_t _state_uninitialized;
    fsm::state_ptr_t _state_initializing;
    fsm::state_ptr_t _state_app_init;
    fsm::state_ptr_t _state_update_init;
    fsm::state_ptr_t _state_running;
    fsm::state_ptr_t _state_shutting_down;
    fsm::state_ptr_t _state_exit_requested;
    fsm::state_ptr_t _state_draining_queues;
    fsm::state_ptr_t _state_joining_update;
    fsm::state_ptr_t _state_final_cleanup;
    fsm::state_ptr_t _state_terminated;
    fsm::state_ptr_t _state_error;

    // Operation queues (references to global singletons)
    opq::opq_ptr_t _mainq;
    opq::opq_ptr_t _updq;
    opq::opq_ptr_t _conq;

    // UPDATE thread
    Thread _updateThread;
    std::atomic<bool> _update_thread_running{false};

    // Subsystem coordination
    std::map<std::string, subsystem_reg_ptr_t> _registered_subsystems;
    fsm::fsmgroup_ptr_t _subsystem_group;
    mutable std::mutex _subsystem_mutex;

private:
    // Core lifecycle helpers (non-virtual)
    void _initAppCore();
    void _initUpdateThread();
    void _shutdownCore();

    // Subsystem helpers
    std::vector<subsystem_reg_ptr_t> _getIndependentSubsystems();
    std::vector<subsystem_reg_ptr_t> _getReadyToInitSubsystems();
    void _initSubsystem(subsystem_reg_ptr_t reg);
    void _shutdownSubsystem(subsystem_reg_ptr_t reg);
    void _buildShutdownWaves(
        std::vector<std::vector<subsystem_reg_ptr_t>>& waves,
        const std::vector<subsystem_reg_ptr_t>& subsystems
    );
};

} // namespace ork
```

### Core Application FSM States

```
AppLifecycle (Root)
├── UNINITIALIZED
├── INITIALIZING
│   ├── APP_INIT
│   │   ├─ Core: Initialize queues, string pool
│   │   ├─ Extension: onAppInitExtension() hook
│   │   └─ Init DURING_APP_INIT subsystems (network, databases)
│   └── UPDATE_INIT
│       ├─ Core: Spawn UPDATE thread
│       ├─ Init AFTER_UPDATE_INIT subsystems (game logic, ECS)
│       └─ Wait for all subsystems READY
├── RUNNING
│   ├─ Core: Process queues, tick subsystems
│   └─ Extension: onUpdateExtension() hook
├── SHUTTING_DOWN
│   ├── EXIT_REQUESTED
│   │   └─ Core: Disable queues (enqueues now no-op), signal UPDATE to exit
│   ├── JOINING_UPDATE
│   │   └─ Core: Join UPDATE thread
│   ├── DRAINING_QUEUES
│   │   ├─ Core: Drain all queues (process work enqueued before EXIT_REQUESTED)
│   │   └─ Extension: onShutdownExtension() hook (called AFTER queues drained)
│   └── FINAL_CLEANUP
├── TERMINATED
└── ERROR
```

**Key:** Core states handle queue management, threading, subsystem coordination. Extension hooks allow lev2 to add GPU/Audio logic.

---

## Design: ork.lev2::OrkEzApp Extends Application

### EzApp Inheritance

```cpp
// ork.lev2/inc/ork/lev2/ezapp.h

namespace ork::lev2 {

class OrkEzApp : public ork::Application {
public:
    // Factory (follows Orkid pattern)
    static ezapp_ptr_t create(appinitdata_ptr_t initdata);

    ~OrkEzApp() override;

    // Override main loop (adds rendering)
    void mainThreadLoop() override;

protected:
    // Constructor
    OrkEzApp(appinitdata_ptr_t initdata);

    // === OVERRIDE FSM BUILDING ===
    void buildApplicationFsm() override;

    // === OVERRIDE LIFECYCLE HOOKS ===
    void onAppInitExtension() override;
    void onUpdateExtension() override;
    void onShutdownExtension() override;

    // === OVERRIDE SUBSYSTEM HOOKS ===
    void initStaticSubsystems() override;
    void shutdownStaticSubsystems() override;

private:
    // === EzApp-SPECIFIC MEMBERS ===

    // GPU subsystem
    std::shared_ptr<GpuSubsystemFsm> _gpu_subsystem;
    ezmainwin_ptr_t _mainWindow;
    CTXBASE* _ctqt;

    // Audio subsystem
    std::shared_ptr<AudioSubsystemFsm> _audio_subsystem;
    audiodevice_ptr_t _audiodevice;
    audio::singularity::synth_ptr_t _synth;

    // Render thread queue (lev2-specific)
    opq::opq_ptr_t _rthreadq;

    // EzApp states (added to core FSM)
    fsm::state_ptr_t _state_gpu_init;
    fsm::state_ptr_t _state_gpu_context_create;
    fsm::state_ptr_t _state_gpu_resources_load;
    fsm::state_ptr_t _state_audio_init;
    fsm::state_ptr_t _state_synth_init;
    fsm::state_ptr_t _state_audio_device_start;
    fsm::state_ptr_t _state_gpu_cleanup;
    fsm::state_ptr_t _state_audio_shutdown;

    // EzApp lifecycle helpers
    void _initGpu();
    void _initAudio();
    void _renderFrame();
    void _shutdownGpu();
    void _shutdownAudio();

    // Add GPU/Audio states to FSM
    void _addGpuStates();
    void _addAudioStates();

    // User callbacks (protected, for compatibility)
    void_lambda_t _onGpuInit;
    void_lambda_t _onAudioInit;
    void_lambda_t _onGpuExit;
    void_lambda_t _onAudioExit;
    // ... etc
};

} // namespace ork::lev2
```

### EzApp FSM Extension Strategy

EzApp extends the core FSM by **inserting states into the INITIALIZING hierarchy**:

```cpp
void OrkEzApp::buildApplicationFsm() {
    // 1. Call base to build core FSM
    Application::buildApplicationFsm();

    // 2. Insert GPU states between APP_INIT and UPDATE_INIT
    _addGpuStates();

    // 3. Insert Audio states after GPU
    _addAudioStates();

    // 4. Modify JOINING_UPDATE to add GPU/Audio cleanup
    _addCleanupStates();
}

void OrkEzApp::_addGpuStates() {
    // Create GPU hierarchy
    _state_gpu_init = _fsm_data->createState(_state_initializing, "GPU_INIT");
    _state_gpu_context_create = _fsm_data->createState(_state_gpu_init, "GPU_CONTEXT_CREATE");
    _state_gpu_resources_load = _fsm_data->createState(_state_gpu_init, "GPU_RESOURCES_LOAD");

    // Insert into transition chain: APP_INIT → GPU_INIT → AUDIO_INIT → UPDATE_INIT
    _fsm_data->addTransition(_state_app_init, "APP_READY", _state_gpu_context_create);
    _fsm_data->addTransition(_state_gpu_context_create, "CONTEXT_CREATED", _state_gpu_resources_load);

    // Callbacks
    _state_gpu_context_create->_onenter = [this](fsm::fsminstance_ptr_t inst) {
        _initGpu();  // Create window, Vulkan context
        inst->sendEvent("CONTEXT_CREATED");
    };

    _state_gpu_resources_load->_onenter = [this](fsm::fsminstance_ptr_t inst) {
        // Core GPU resources
        _loadCoreGpuResources();

        // User callback (protected)
        if (_onGpuInit) {
            try {
                _onGpuInit(_ctqt);
            } catch (...) {
                logchan_ezapp->log_error("User GPU init failed");
            }
        }

        inst->sendEvent("GPU_READY");
    };
}
```

### Final EzApp FSM Structure

```
AppLifecycle (Root)
├── UNINITIALIZED
├── INITIALIZING
│   ├── APP_INIT (core)
│   │   └── Init DURING_APP_INIT subsystems
│   ├── GPU_INIT (EzApp adds) ← INSERTED HERE
│   │   ├── GPU_CONTEXT_CREATE
│   │   └── GPU_RESOURCES_LOAD
│   │       └── Init AFTER_GPU_INIT subsystems
│   ├── AUDIO_INIT (EzApp adds) ← INSERTED HERE
│   │   ├── SYNTH_INIT
│   │   └── AUDIO_DEVICE_START
│   │       └── Init AFTER_AUDIO_INIT subsystems
│   └── UPDATE_INIT (core)
│       └── Init AFTER_UPDATE_INIT subsystems
├── RUNNING (both use)
│   └── onUpdateExtension() renders frame
├── SHUTTING_DOWN
│   ├── EXIT_REQUESTED (core)
│   │   ├── Disable queues (enqueues no-op)
│   │   ├── Shutdown BEFORE_UPDATE_EXIT subsystems
│   │   └── Signal UPDATE to exit
│   ├── JOINING_UPDATE (core)
│   │   ├── _updateThread.join() ← UPDATE THREAD JOINS FIRST
│   │   └── Shutdown BEFORE_QUEUES_DRAIN subsystems
│   ├── DRAINING_QUEUES (core)
│   │   ├── Drain remaining work ← DRAIN AFTER UPDATE TERMINATED
│   │   └── onShutdownExtension() → calls:
│   │       ├─── Shutdown BEFORE_AUDIO_SHUTDOWN subsystems
│   │       ├─── AUDIO_SHUTDOWN (EzApp adds)
│   │       ├─── Shutdown BEFORE_GPU_CLEANUP subsystems
│   │       └─── GPU_CLEANUP (EzApp adds)
│   └── FINAL_CLEANUP (core)
│       └── Shutdown DURING_FINAL_CLEANUP subsystems
├── TERMINATED
└── ERROR
```

---

## SubsystemFsm Base Class (in core)

```cpp
// ork.core/inc/ork/application/subsystem.h

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// SubsystemFsm - Base class for application subsystems
// Provides standard lifecycle states
///////////////////////////////////////////////////////////////////////////////

class SubsystemFsm {
public:
    SubsystemFsm(const std::string& name);
    ~SubsystemFsm() = default;

    // Called by factory functions to set up FSM states
    void initialize();

    // Shutdown subsystem
    void shutdown();

    // Query
    fsm::state_ptr_t currentState() const;
    const std::string& name() const { return _name; }

    // FSM access (protected members for derived classes)
    fsm::fsminstance_ptr_t _instance;
    fsm::fsmdata_ptr_t _data;
    std::string _name;

    // Standard subsystem states (cached for convenience)
    fsm::state_ptr_t _state_uninitialized;
    fsm::state_ptr_t _state_initializing;
    fsm::state_ptr_t _state_ready;
    fsm::state_ptr_t _state_shutting_down;
    fsm::state_ptr_t _state_terminated;
    fsm::state_ptr_t _state_error;
};

///////////////////////////////////////////////////////////////////////////////
// Note: Initialization and shutdown order is IMPLICIT from dependency graph
// No need for explicit init/shutdown phase enums - dependencies determine everything!
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// SubsystemRegistration - Metadata for registered subsystems
///////////////////////////////////////////////////////////////////////////////

struct SubsystemRegistration {
    uint64_t name_hash;                     // CRC hash of subsystem name
    std::string name;                       // "gpu", "audio", "ecs" (for debugging)
    subsystemfsm_ptr_t subsystem;
    bool is_static = false;                 // Static = persists until app exit
    std::atomic<bool> is_initializing{false};
    std::atomic<bool> is_shutting_down{false};

    // Note: Dependencies stored in subsystem->_dependencies map
    // Init/shutdown order is IMPLICIT from dependency graph
};

} // namespace ork
```

### GPU/Audio Subsystems (in lev2)

```cpp
// ork.lev2/inc/ork/lev2/gpu_subsystem.h

namespace ork::lev2 {

// GPU-specific implementation (pimpl)
struct GpuSubsystemImpl {
    CTXBASE* _context = nullptr;
    ezmainwin_ptr_t _window;
};

// GPU subsystem factory function
subsystemfsm_ptr_t createGpuSubsystem();

// Audio-specific implementation (pimpl)
struct AudioSubsystemImpl {
    audiodevice_ptr_t _device;
    audio::singularity::synth_ptr_t _synth;
};

// Audio subsystem factory function
subsystemfsm_ptr_t createAudioSubsystem();

} // namespace ork::lev2
```

---

## Usage Examples

### Server-Side Application (core only, no graphics)

```cpp
// server_app.cpp - Using ork.core only

#include <ork/application/application.h>
#include <ork/application/subsystem.h>

// Network-specific implementation (pimpl)
struct NetworkSubsystemImpl {
    socket_ptr_t _socket;
};

// Network subsystem factory function
subsystemfsm_ptr_t createNetworkSubsystem() {
    auto subsystem = std::make_shared<ork::SubsystemFsm>("network");

    // Store impl using svar64_t (pimpl pattern)
    auto impl = new NetworkSubsystemImpl();
    subsystem->_impl.set<NetworkSubsystemImpl*>(impl);

    // Build FSM states
    subsystem->_data = std::make_shared<fsm::FsmData>();
    subsystem->_state_uninitialized = subsystem->_data->createState(nullptr, "UNINITIALIZED");
    subsystem->_state_initializing = subsystem->_data->createState(nullptr, "INITIALIZING");
    subsystem->_state_ready = subsystem->_data->createState(nullptr, "READY");
    subsystem->_state_shutting_down = subsystem->_data->createState(nullptr, "SHUTTING_DOWN");
    subsystem->_state_terminated = subsystem->_data->createState(nullptr, "TERMINATED");

    // Setup transitions
    subsystem->_data->addTransition(subsystem->_state_uninitialized, "START", subsystem->_state_initializing);
    subsystem->_data->addTransition(subsystem->_state_initializing, "INIT_DONE", subsystem->_state_ready);
    subsystem->_data->addTransition(subsystem->_state_ready, "SHUTDOWN", subsystem->_state_shutting_down);
    subsystem->_data->addTransition(subsystem->_state_shutting_down, "CLEANUP_DONE", subsystem->_state_terminated);

    // Initialize callback
    subsystem->_state_initializing->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        // Initialize network sockets
        impl->_socket = createServerSocket(8080);
        inst->sendEvent("INIT_DONE");
    };

    // Shutdown callback
    subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        delete impl;
    };

    // Create FSM instance
    subsystem->_instance = fsm::FsmInstance::create(subsystem->_data);
    subsystem->_instance->changeState(subsystem->_state_uninitialized);

    return subsystem;
}

int main(int argc, char** argv) {
    // Create core application (NO lev2 dependency)
    auto initdata = std::make_shared<ork::AppInitData>(argc, argv);
    auto app = ork::Application::create(initdata);

    // Register network subsystem (static, no dependencies)
    auto network = createNetworkSubsystem();
    app->registerSubsystem("network", network, true);

    // Run server loop (no rendering, just queue processing)
    app->mainThreadLoop();

    return 0;
}
```

### GUI Application (lev2, with graphics)

```cpp
// gui_app.cpp - Using ork.lev2

#include <ork/lev2/ezapp.h>
#include <ork/ecs/subsystem.h>

class MyGuiApp {
public:
    void run() {
        // Create lev2 application (extends core Application)
        auto initdata = std::make_shared<ork::AppInitData>(argc, argv);
        auto app = lev2::OrkEzApp::create(initdata);

        // Register static subsystems (GPU, Audio registered by EzApp automatically)
        // ... GPU and Audio are built-in to EzApp ...

        // User can still add custom subsystems (no dependencies)
        auto network = std::make_shared<NetworkSubsystemFsm>();
        network->initialize();
        app->registerSubsystem("network", network, true);

        // Run GUI loop (includes rendering)
        app->mainThreadLoop();
    }
};
```

### Dynamic ECS Per Level (works with both core and lev2)

```cpp
// Level management (same pattern for server or GUI app)

void loadLevel(ork::application_ptr_t app, const std::string& level_name) {
    std::string ecs_name = "ecs_" + level_name;

    // Create ECS subsystem
    auto ecs = ork::ecs::createEcsSubsystem();

    // Set up dependencies via _dependencies map
    // ECS depends on GPU if using lev2, or nothing if core-only
    auto gpu_subsystem = app->findSubsystem("gpu");
    if (gpu_subsystem) {
        ecs->_dependencies["gpu"_crcu] = gpu_subsystem;
    }

    // Register dynamically (order is IMPLICIT from dependencies)
    app->registerSubsystem(ecs_name, ecs, false);  // is_static = false
}

void unloadLevel(ork::application_ptr_t app, const std::string& level_name) {
    app->unregisterSubsystem("ecs_" + level_name);
}
```

---

## Implementation Strategy

### Phase 1: Create Core Application Class (3-5 days)
- [ ] Create `ork.core/inc/ork/application/application.h`
- [ ] Implement `Application` base class with FSM
- [ ] Build core states (INIT, RUNNING, SHUTDOWN)
- [ ] Implement subsystem registration (from apphfsm.md)
- [ ] Implement OPQ queue management
- [ ] Implement UPDATE thread management
- [ ] Add virtual hooks for extension
- [ ] Write unit tests (server-only app)

### Phase 2: Create SubsystemFsm Base (2-3 days)
- [ ] Create `ork.core/inc/ork/application/subsystem.h`
- [ ] Implement `SubsystemFsm` base class
- [ ] Implement `SubsystemRegistration` struct
- [ ] Test with example network subsystem
- [ ] Add Python bindings for core Application

### Phase 3: Refactor EzApp to Extend Application (5-7 days)
- [ ] Change `OrkEzApp` to inherit from `ork::Application`
- [ ] Override `buildApplicationFsm()` to add GPU/Audio states
- [ ] Override lifecycle hooks (onAppInitExtension, etc.)
- [ ] Move GPU-specific code to GpuSubsystemFsm
- [ ] Move Audio-specific code to AudioSubsystemFsm
- [ ] Test that existing lev2 apps still work
- [ ] Ensure backward compatibility with current callback API

### Phase 4: Update Documentation (1-2 days)
- [ ] Update `apphfsm.md` to reflect core/lev2 split
- [ ] Document core Application API
- [ ] Document extension pattern for custom app types
- [ ] Add server-side application examples
- [ ] Update application_framework_tdd.md

### Phase 5: Migration and Testing (3-5 days)
- [ ] Test server-only applications
- [ ] Test GUI applications
- [ ] Test dynamic subsystem registration
- [ ] Test multiple ECS instances
- [ ] Validate all subsystem coordination
- [ ] Performance benchmarking

**Total Estimated Time:** 3-4 weeks

---

## Benefits of Layered Architecture

### For Core-Only Apps (Servers, Daemons, CLI Tools)
✅ Standard lifecycle framework without graphics bloat
✅ FSM-based deterministic shutdown
✅ Subsystem coordination (network, database, etc.)
✅ OPQ queue management built-in
✅ Thread coordination (MAIN + UPDATE)
✅ No dependency on OpenGL/Vulkan/GLFW/Audio libraries

### For GUI Apps (Games, Tools, Editors)
✅ All core benefits PLUS GPU/Audio/UI
✅ Clean inheritance hierarchy
✅ Easy to extend with custom subsystems
✅ Backward compatible with existing EzApp API
✅ Dynamic subsystem lifecycle (ECS per level)

### For Framework Design
✅ Clear separation of concerns
✅ Core has no lev2 dependencies (no circular deps)
✅ Easy to test (core can be tested independently)
✅ Extensible via virtual hooks (template method pattern)
✅ Follows Orkid patterns (factories, shared_ptr, FSM)

---

## Migration Path for Existing Code

### No Breaking Changes
Existing `OrkEzApp` code continues to work:

```python
# Existing Python app - NO CHANGES NEEDED
class MyApp(ComponentizedApplication):
    def _onGpuInit(self, ctx):
        # Still works!
        pass
```

The only change is **internal** - EzApp now inherits from Application.

### Opt-In Core Usage
New server apps can opt-in to core-only:

```cpp
// NEW: Server app using core only
auto app = ork::Application::create(initdata);
app->mainThreadLoop();  // No graphics, just queues
```

---

## Open Questions

1. **Should core Application support "headless GPU"?**
   - **Proposal:** No, keep core truly graphics-free. Use lev2 with offscreen rendering.

2. **Should FREERUN/LOCKSTEP modes be in core or lev2?**
   - **Proposal:** Lev2 (rendering-specific). Core just has RUNNING state.

3. **Should core have a "ConsoleApp" class between Application and EzApp?**
   - **Proposal:** Phase 2 feature - Application is enough for MVP.

4. **How to handle Python bindings split?**
   - **Proposal:**
     - `orkengine.core.Application` - core bindings
     - `orkengine.lev2.OrkEzApp` (extends Application) - lev2 bindings

---

## Summary

This layered architecture provides:

1. **Core Application** (ork.core) - FSM lifecycle for all apps
2. **EzApp** (ork.lev2) - Extends with GPU/Audio/UI
3. **Clean separation** - No circular dependencies
4. **Backward compatible** - Existing code continues to work
5. **Server-side ready** - ork.core can be used standalone

**Next Step:** Review this design, iterate, then implement Phase 1 (Core Application class).

---

**Status:** Design Phase - Core Layering Proposal
**Last Updated:** 2026-01-12
