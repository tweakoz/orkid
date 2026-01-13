# HFSM-Based Application Lifecycle Design

## Executive Summary

This document proposes replacing OrkEzApp's state-flag-based lifecycle management with a Hierarchical Finite State Machine (HFSM) to ensure deterministic, race-free shutdown across all subsystems (GPU/Vulkan, audio, threads, operation queues, ECS, network sockets, etc.).

**IMPORTANT:** This document describes the complete application lifecycle system.

**Related Documents:**
- **[apphfsm_core_layering.md](./apphfsm_core_layering.md)** - Architectural split between `ork.core` and `ork.lev2`

### Architecture Overview

The HFSM application lifecycle is layered across two modules:

| Module | Responsibility | Usage |
|--------|---------------|-------|
| **ork.core** | Base Application class, FSM lifecycle, Subsystem, queues, threads, dependency management | Server-side apps, daemons, CLI tools (no graphics) |
| **ork.lev2** | OrkEzApp extends Application, adds GPU/Audio/UI subsystems | GUI apps, games, editors (with graphics) |

**Key Benefits:**
- **Unified Subsystem Pattern**: All resources (GPU, Audio, Physics, Network) are Subsystem instances with factory functions
- **Dependency-Driven Ordering**: Init/shutdown order computed from dependency graph (no explicit phases)
- **Thread-Safe Registration**: Subsystems can be registered dynamically during RUNNING
- **Core-only Apps**: Servers and background services can use the HFSM lifecycle without graphics dependencies

**Design Goals:**
1. **Deterministic Shutdown** - Explicit state transitions prevent race conditions
2. **Clean Resource Cleanup** - RAII patterns with guaranteed ordering
3. **Subsystem Coordination** - FsmGroup ensures all subsystems ready before transitions
4. **Error Recovery** - Explicit error states with recovery paths
5. **FSM-Owned Lifecycle** - FSM controls all critical operations, user callbacks are protected extensions
6. **Guaranteed Forward Progress** - System cannot hang on user callback failures
7. **Debuggability** - DOT graph generation shows current application state
8. **Python Integration** - Full Python visibility into application lifecycle

---

## Design Philosophy: FSM-Owned Lifecycle

### Core Principle

**The FSM owns and controls the lifecycle. User callbacks are optional protected extensions, not lifecycle controllers.**

This is a fundamental shift from the current callback-driven approach:

| Aspect | Current (Callback-Driven) | New (FSM-Owned) |
|--------|--------------------------|-----------------|
| **Lifecycle Control** | User callbacks control flow | FSM controls flow |
| **Failure Mode** | User callback hang = system hang | User callback hang = timeout → error state |
| **Responsibility** | User must signal completion | FSM automatically signals completion |
| **Error Handling** | Unhandled exception crashes | Exception caught → error state transition |
| **Validation** | User must ensure correctness | FSM validates postconditions |
| **Progress Guarantee** | None (can hang forever) | FSM guarantees forward progress |

### Protection Mechanisms

Every user callback is wrapped with protection:

```cpp
// FSM state handler pattern
_state_gpu_init->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    // 1. FSM does essential initialization (cannot fail)
    bool core_success = _initGpuCore();
    if (!core_success) {
        logchan_ezapp->log_error("Core GPU init failed");
        inst->sendEvent("GPU_INIT_FAILED");
        return;
    }

    // 2. Optional user callback (protected)
    if (_onGpuInit) {
        try {
            _onGpuInit(_mainWindow->_ctqt);
        } catch (const std::exception& e) {
            logchan_ezapp->log_error("User GPU init callback failed: %s", e.what());
            // Continue anyway - core init succeeded
        } catch (...) {
            logchan_ezapp->log_error("User GPU init callback failed with unknown exception");
        }
    }

    // 3. Validate postconditions
    if (!_validateGpuState()) {
        logchan_ezapp->log_error("GPU validation failed");
        inst->sendEvent("GPU_INIT_FAILED");
        return;
    }

    // 4. FSM guarantees forward progress
    inst->sendEvent("GPU_READY");
};
```

**Key Benefits:**

✅ **User code cannot hang the system** - FSM always proceeds
✅ **User exceptions are isolated** - Cannot crash the application
✅ **Core initialization always happens** - Minimum viable system guaranteed
✅ **Automatic event signaling** - User doesn't have to remember
✅ **Clear contracts** - User callbacks are documented as "optional extensions"

### User Callback Contract

When users implement callbacks in Python or C++, the contract is:

```python
class MyApp(ComponentizedApplication):
    def _onGpuInit(self, ctx):
        """Optional callback during GPU initialization.

        The FSM has already:
        - Created the GPU context
        - Initialized Vulkan/OpenGL
        - Set up core resources

        Use this to:
        - Load your application-specific resources
        - Set up custom render pipelines
        - Configure GPU state

        Note:
        - If this raises an exception, FSM logs the error and continues
        - You do NOT need to signal completion - FSM handles that
        - The system is already in a working state when this is called
        - This is called on the MAIN/GPU thread
        """
        # Your optional initialization here
        self.my_texture = ctx.loadTexture("my_texture.png")
```

### Failure Modes and Handling

| User Callback Behavior | FSM Response | Application State |
|------------------------|--------------|-------------------|
| **Succeeds normally** | Continue to next state | Fully functional |
| **Throws exception** | Log error, continue | Core functional, user features may be degraded |
| **Hangs (future)** | Timeout, transition to error state | Core functional, can retry or exit |
| **Not provided** | Skip callback, continue | Core functional |

### Future Enhancement: Timeout Protection

**Phase 10 (Post-MVP):** Add timeout protection for callbacks that might hang:

```cpp
// Future enhancement: async callback execution with timeout
_state_gpu_init->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    // Core init
    if (!_initGpuCore()) {
        inst->sendEvent("GPU_INIT_FAILED");
        return;
    }

    // User callback with timeout (30 seconds)
    if (_onGpuInit) {
        auto future = std::async(std::launch::async, [&]() {
            try {
                _onGpuInit(_mainWindow->_ctqt);
            } catch (...) {
                // Exceptions propagated to future
                throw;
            }
        });

        auto status = future.wait_for(std::chrono::seconds(30));

        if (status == std::future_status::timeout) {
            logchan_ezapp->log_error("GPU init callback timeout (30s)");
            inst->sendEvent("GPU_INIT_TIMEOUT");  // Recoverable error
            return;
        }

        try {
            future.get();  // Rethrow exceptions if any
        } catch (const std::exception& e) {
            logchan_ezapp->log_error("GPU init callback failed: %s", e.what());
            // Continue anyway
        }
    }

    // Validation and completion
    if (_validateGpuState()) {
        inst->sendEvent("GPU_READY");
    } else {
        inst->sendEvent("GPU_INIT_FAILED");
    }
};
```

**Considerations:**

- **Timeout values:** Should be configurable per callback type (init vs. frame update)
- **Thread safety:** Async execution requires careful GIL management for Python
- **Cancellation:** How to cancel a hung callback? (OS threads can't be safely killed)
- **Testing:** Simulate hangs with sleep() calls in test callbacks

**MVP Decision:** Start **without** timeout protection. Add in Phase 10 if needed. Rationale:
1. Exception protection is more critical (immediate issue)
2. Timeout adds significant complexity (async execution, thread management)
3. Proper implementation requires careful design for Python GIL
4. Can be added incrementally without breaking existing code

---

## Current System Analysis

### Problems with Flag-Based Approach

The current system uses atomic flags (`KAPPSTATEFLAG_UPDRUNNING`, `KAPPSTATEFLAG_JOINING`, `KAPPSTATEFLAG_JOINED`) with implicit synchronization:

```cpp
// Current approach (ezapp.cpp:527, 692)
while (not checkAppState(KAPPSTATEFLAG_JOINING)) {
    // UPDATE loop
}
```

**Issues Identified:**

1. **Race Conditions:**
   - Flag check and queue processing not atomic
   - UPDATE thread could be processing work while MAIN sets JOINING flag
   - No explicit memory barriers between state flag writes and resource cleanup

2. **Arbitrary Limits:**
   - `joinUpdate()` drains `_mainq` only 100 iterations (line 341)
   - No guarantee all pending work processed before shutdown

3. **Implicit Ordering:**
   - Shutdown sequence documented in comments, not enforced
   - Easy to violate constraints (e.g., UPDATE must exit before GPU cleanup)

4. **No Error States:**
   - If audio init fails, system continues in undefined state
   - No recovery path for subsystem failures

5. **Timing Issues:**
   - `DrawQueue::ClearAndSyncWriters()` called AFTER thread join (line 347)
   - Secondary window cleanup timing unclear
   - `gloadercontext` lifetime management unclear

6. **Edge Cases:**
   - Headless mode (no GPU) creates different paths
   - Movie recording finish called from arbitrary thread
   - `atexit()` handlers run after static destructors

---

## Proposed HFSM Architecture

### High-Level State Hierarchy

The application uses a **dependency-driven subsystem model** where init/shutdown order is determined by the dependency graph, not hardcoded phases.

```
AppLifecycle (Root)
├── UNINITIALIZED
├── INITIALIZING
│   ├── APP_INIT (core infrastructure: queues, string pool, basic setup)
│   │
│   └── SUBSYSTEM_INIT (dependency-driven, parallel waves)
│       │
│       ├─── Wave 1: Subsystems with NO dependencies (init in parallel)
│       │    Examples: Network, Database, Core Services
│       │
│       ├─── Wave 2: Subsystems depending on Wave 1 (init in parallel)
│       │    Examples: GPU (if no deps), Audio (if no deps), UPDATE thread
│       │
│       ├─── Wave 3: Subsystems depending on Wave 2 (init in parallel)
│       │    Examples: Physics (→UPDATE), Rendering (→GPU), DSP (→Audio)
│       │
│       └─── Wave N: Continue until all subsystems initialized
│            Examples: ECS (→GPU+Physics), UI (→GPU), Game Logic (→multiple)
│
├── RUNNING
│   ├── FREERUN (async update/render)
│   └── LOCKSTEP (sync update/render)
│
├── SHUTTING_DOWN (reverse dependency order)
│   ├── EXIT_REQUESTED (signal all subsystems, disable queues)
│   ├── SUBSYSTEM_SHUTDOWN (dependency-driven, parallel waves - REVERSE order)
│   │   ├─── Wave 1: Leaf subsystems (no dependents)
│   │   │    Examples: ECS, UI, Game Logic
│   │   ├─── Wave 2: Mid-level subsystems
│   │   │    Examples: Physics, Rendering, DSP
│   │   └─── Wave N: Root subsystems (many dependents)
│   │        Examples: GPU, Audio, UPDATE thread, Network
│   ├── DRAINING_QUEUES (drain all operation queues)
│   └── FINAL_CLEANUP (core cleanup: close windows, release contexts)
│
├── TERMINATED
└── ERROR
    ├── INIT_FAILED
    ├── SUBSYSTEM_ERROR (GPU_LOST, NETWORK_ERROR, etc.)
    └── UNRECOVERABLE
```

### Core States vs Subsystem States

**Core Application States (ork.core):**
- `UNINITIALIZED`, `INITIALIZING`, `RUNNING`, `SHUTTING_DOWN`, `TERMINATED`, `ERROR`
- Fixed lifecycle managed by `Application` FSM
- All apps have these regardless of subsystems

**Subsystem-Specific States (per Subsystem instance):**
- Each subsystem (GPU, Audio, Physics, Network, ECS, etc.) has its own FSM
- Example: GPU subsystem states: `UNINITIALIZED`, `INITIALIZING`, `READY`, `TERMINATED`
- Example: Audio subsystem states: `UNINITIALIZED`, `SYNTH_INIT`, `DEVICE_START`, `READY`, `TERMINATED`
- Subsystem init/shutdown order determined by dependency graph (topological sort)

### Example: Typical GUI Application Init Sequence

**Dependency Graph:**
```
Network: no deps
GPU: no deps
Audio: no deps
UPDATE: no deps
Physics: depends on UPDATE
Rendering: depends on GPU
DSP: depends on Audio
ECS: depends on GPU + Physics
```

**Initialization (dependency-driven waves):**
```
APP_INIT:
  - Setup queues (_mainq, _updq, _conq)
  - Initialize string pool
  - Execute pre-init operations
  ↓
Wave 1 (parallel):
  - Network subsystem → READY
  - GPU subsystem → READY (context creation, resource load)
  - Audio subsystem → READY (synth init, device start)
  - UPDATE subsystem → READY (spawn thread)
  ↓
Wave 2 (parallel, waiting for Wave 1):
  - Physics subsystem (→UPDATE) → READY
  - Rendering subsystem (→GPU) → READY
  - DSP subsystem (→Audio) → READY
  ↓
Wave 3 (parallel, waiting for Wave 2):
  - ECS subsystem (→GPU+Physics) → READY
  ↓
RUNNING:
  - All subsystems ready
  - Main loop processing
```

### Thread Activity During RUNNING States

All threads continuously process **transient subsystem callbacks** via operation queues:

| Thread      | Operation Queue | Callback Lifecycle |
|-------------|-----------------|-------------------|
| **UPDATE**  | `updq` | `onUpdateInit` → `onUpdate` (loop) → `onUpdateExit` |
| **MAIN/GPU** | `mainq` | `onGpuInit` → `onGpuUpdate` (loop) → `onGpuExit` |
| **AUDIO**   | `conq` | `onAudioInit` → `onAudioUpdate` (loop) → `onAudioExit` |
| **USER**    | `mainq` | `onInit` → `onUpdate` (loop) → `onExit` |

**Transient Subsystems:**
- Dynamically registered during RUNNING (e.g., level load, mod load)
- Each subsystem's callbacks enqueued to appropriate thread's OPQ
- Init fires once on registration
- Update fires every frame/tick
- Exit fires on unregistration

**FREERUN vs LOCKSTEP:**
- **FREERUN**: Threads run independently (async)
- **LOCKSTEP**: UPDATE thread waits for GPU, GPU waits for UPDATE (sync)

**Shutdown (reverse dependency-driven waves):**
```
EXIT_REQUESTED:
  - Signal all subsystems to stop accepting work
  - Disable operation queues (enqueues become no-ops)
  ↓
Wave 1 (parallel - leaf subsystems):
  - ECS subsystem → TERMINATED (no dependents)
  ↓
Wave 2 (parallel):
  - Physics subsystem → TERMINATED
  - Rendering subsystem → TERMINATED
  - DSP subsystem → TERMINATED
  ↓
Wave 3 (parallel):
  - GPU subsystem → TERMINATED
  - Audio subsystem → TERMINATED
  - UPDATE subsystem → TERMINATED (thread joins)
  - Network subsystem → TERMINATED
  ↓
DRAINING_QUEUES:
  - Process remaining operations in all queues
  ↓
FINAL_CLEANUP:
  - Close windows, release core resources
  ↓
TERMINATED
```

### State Descriptions

| State | Responsibility | Entry Condition | Exit Condition |
|-------|----------------|-----------------|----------------|
| **UNINITIALIZED** | Before any init | Process start | `START_APP` event |
| **APP_INIT** | Core infrastructure setup (queues, string pool) | Start event received | Core init complete |
| **SUBSYSTEM_INIT** | Initialize all registered subsystems in dependency order waves | APP_INIT complete | All subsystems READY |
| **RUNNING** | Main application loop, all subsystems active | All subsystems ready | Exit signal received |
| **FREERUN** | Async update/render (update/render on different threads) | RUNNING + mode select | Mode change or exit |
| **LOCKSTEP** | Sync update/render (render waits for update) | RUNNING + mode select | Mode change or exit |
| **EXIT_REQUESTED** | Signal shutdown, disable queues | Exit signal | All subsystems signaled |
| **SUBSYSTEM_SHUTDOWN** | Shutdown subsystems in reverse dependency order waves | Exit requested | All subsystems terminated |
| **DRAINING_QUEUES** | Process remaining queued operations | Subsystems shut down | All queues empty |
| **FINAL_CLEANUP** | Core cleanup (windows, contexts, app-level resources) | Queues drained | Core resources released |
| **TERMINATED** | All resources released, ready for process exit | Final cleanup done | Process exit |
| **ERROR** | Error handling and recovery | Error detected | Recovery or exit |

---

## Component Structure

**See [apphfsm_core_layering.md](./apphfsm_core_layering.md) for the complete architectural design.**

This section provides an overview of the key components. The actual implementation is split between `ork.core` (base) and `ork.lev2` (GPU/Audio extensions).

### 1. Application - Base Application Class (ork.core)

Located in `ork.core/inc/ork/application/application.h`:

```cpp
namespace ork {

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
        subsystem_ptr_t subsystem,
        bool is_static = false
    );

    // Convenience overload for string names (auto-hashes to uint64_t)
    void registerSubsystem(
        const std::string& name,
        subsystem_ptr_t subsystem,
        bool is_static = false
    );

    void unregisterSubsystem(const std::string& name);
    subsystem_ptr_t findSubsystem(const std::string& name) const;

    // Shutdown
    virtual void signalExit();

    // Access
    fsm::fsminstance_ptr_t getFsm() const { return _app_fsm; }
    opq::opq_ptr_t getMainQueue() const { return _mainq; }
    opq::opq_ptr_t getUpdateQueue() const { return _updq; }
    opq::opq_ptr_t getConcurrentQueue() const { return _conq; }

protected:
    Application(appinitdata_ptr_t initdata);

    // Virtual hooks for derived classes (lev2 overrides these)
    virtual void buildApplicationFsm();
    virtual void onAppInitExtension() {}
    virtual void onUpdateExtension() {}
    virtual void onShutdownExtension() {}

    // FSM and state references
    fsm::fsminstance_ptr_t _app_fsm;
    fsm::fsmdata_ptr_t _fsm_data;
    fsm::state_ptr_t _state_uninitialized;
    fsm::state_ptr_t _state_initializing;
    fsm::state_ptr_t _state_app_init;
    fsm::state_ptr_t _state_update_init;
    fsm::state_ptr_t _state_running;
    fsm::state_ptr_t _state_shutting_down;
    fsm::state_ptr_t _state_terminated;
    fsm::state_ptr_t _state_error;

    // Operation queues
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
    void _initAppCore();
    void _initUpdateThread();
    void _shutdownCore();
    // ... subsystem helpers ...
};

} // namespace ork
```

### 1a. OrkEzApp - GUI Application (ork.lev2)

Located in `ork.lev2/inc/ork/lev2/ezapp.h`:

```cpp
namespace ork::lev2 {

class OrkEzApp : public ork::Application {
public:
    static ezapp_ptr_t create(appinitdata_ptr_t initdata);
    ~OrkEzApp() override;

    void mainThreadLoop() override;  // Adds rendering

protected:
    OrkEzApp(appinitdata_ptr_t initdata);

    // Override FSM building to add GPU/Audio states
    void buildApplicationFsm() override;

    // Override lifecycle hooks
    void onAppInitExtension() override;
    void onUpdateExtension() override;
    void onShutdownExtension() override;

private:
    // GPU subsystem
    std::shared_ptr<GpuSubsystem> _gpu_subsystem;
    ezmainwin_ptr_t _mainWindow;
    CTXBASE* _ctqt;

    // Audio subsystem
    std::shared_ptr<AudioSubsystem> _audio_subsystem;
    audiodevice_ptr_t _audiodevice;
    audio::singularity::synth_ptr_t _synth;

    // Render thread queue (lev2-specific)
    opq::opq_ptr_t _rthreadq;

    // EzApp-specific states (added to core FSM)
    fsm::state_ptr_t _state_gpu_init;
    fsm::state_ptr_t _state_audio_init;
    fsm::state_ptr_t _state_gpu_cleanup;
    fsm::state_ptr_t _state_audio_shutdown;

    // User callbacks (for backward compatibility)
    void_lambda_t _onGpuInit;
    void_lambda_t _onAudioInit;
    void_lambda_t _onGpuExit;
    void_lambda_t _onAudioExit;
};

} // namespace ork::lev2
```

### 2. Subsystem - Base Class for Subsystems (ork.core)

Located in `ork.core/inc/ork/application/subsystem.h`:

```cpp
namespace ork {

// Note: Initialization and shutdown order is IMPLICIT from dependency graph
// No need for explicit init/shutdown phase enums - dependencies determine everything!

// Base class for all subsystems (core, lev2, or user-defined)
// GPU, Audio, Physics, Network, etc. are all Subsystem instances
// No subclassing needed - use factory functions with pimpl pattern
class Subsystem {
public:
    Subsystem(const std::string& name);
    ~Subsystem() = default;

    // Called by factory functions to set up FSM states
    void initialize();
    void shutdown();

    fsm::state_ptr_t currentState() const;
    const std::string& name() const { return _name; }
    uint64_t nameHash() const { return _name_hash; }

    // Orkid patterns - implementation storage
    svar64_t _impl;          // Pimpl - implementation-specific data
    varmap_ptr_t _vars;      // Variable map for properties

    // Subsystem identity
    uint64_t _name_hash;     // CRC hash of name (e.g., "gpu"_crcu)
    std::string _name;       // "gpu", "audio", "physics" (for debugging)

    // Dependencies - pointer map keyed by hash
    std::unordered_map<uint64_t, subsystem_ptr_t> _dependencies;

    // FSM access (public for configuration)
    fsm::fsminstance_ptr_t _instance;
    fsm::fsmdata_ptr_t _data;

    // Standard subsystem states (cached)
    fsm::state_ptr_t _state_uninitialized;
    fsm::state_ptr_t _state_initializing;
    fsm::state_ptr_t _state_ready;
    fsm::state_ptr_t _state_shutting_down;
    fsm::state_ptr_t _state_terminated;
    fsm::state_ptr_t _state_error;
};

struct SubsystemRegistration {
    uint64_t name_hash;                     // CRC hash of subsystem name
    std::string name;                       // "gpu", "audio", "ecs" (for debugging)
    subsystem_ptr_t subsystem;
    bool is_static = false;                 // Static subsystems persist until app exit
    std::atomic<bool> is_initializing{false};
    std::atomic<bool> is_shutting_down{false};

    // Note: Dependencies stored in subsystem->_dependencies map
    // Init/shutdown order is IMPLICIT from dependency graph:
    // - Init: Wait for all dependencies to be ready, then initialize
    // - Shutdown: Shutdown this first, then dependencies (reverse order)
};

} // namespace ork
```

### 2a. Example Subsystems - Everything is Subsystem

**GPU, Audio, Physics, Network, etc. are all just Subsystem instances.**

**Dependency Chain:**

```
OPQ (subsystem)
  └─ deps: []

CORE (subsystem)
  └─ deps: [OPQ]

GPU (subsystem) *optional*
  └─ deps: [CORE]

AUDIO (subsystem) *optional*
  └─ deps: [CORE]

UPDATE (subsystem)
  └─ deps: [CORE, ?GPU, ?AUDIO]  // GPU/AUDIO optional

USER_PHYSICS (subsystem)
  └─ deps: [UPDATE]

USER_ECS (subsystem)
  └─ deps: [UPDATE, GPU, USER_PHYSICS]
```

### 2b. GPU Subsystem Example (ork.lev2)

```cpp
namespace ork::lev2 {

// GPU-specific implementation (pimpl)
struct GpuSubsystemImpl {
    lev2::Context* _context = nullptr;
    ezmainwin_ptr_t _window;
    // All GPU-specific data here

    // Helper methods
    ezmainwin_ptr_t createWindow();
    lev2::Context* createContext(ezmainwin_ptr_t window);
};

// GPU subsystem factory function (lev2-specific)
subsystem_ptr_t createGpuSubsystem() {
    auto subsystem = std::make_shared<Subsystem>("gpu");

    // Store impl using svar64_t (pimpl pattern)
    auto impl = new GpuSubsystemImpl();
    subsystem->_impl.set<GpuSubsystemImpl*>(impl);

    // Build FSM states
    subsystem->_data = std::make_shared<fsm::FsmData>();
    subsystem->_state_uninitialized = subsystem->_data->createState(nullptr, "UNINITIALIZED");
    subsystem->_state_initializing = subsystem->_data->createState(nullptr, "INITIALIZING");
    subsystem->_state_ready = subsystem->_data->createState(nullptr, "READY");
    subsystem->_state_terminated = subsystem->_data->createState(nullptr, "TERMINATED");

    // Setup transitions
    subsystem->_data->addTransition(subsystem->_state_uninitialized, "START", subsystem->_state_initializing);
    subsystem->_data->addTransition(subsystem->_state_initializing, "INIT_DONE", subsystem->_state_ready);

    // Initialize callback
    subsystem->_state_initializing->_onenter = [subsystem, impl](fsm::fsminstance_ptr_t inst) {
        // Create GPU context
        impl->_window = impl->createWindow();
        impl->_context = impl->createContext(impl->_window);
        inst->sendEvent("INIT_DONE");
    };

    // Shutdown callback
    subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        impl->_context->release();
        delete impl;
    };

    // Create FSM instance
    subsystem->_instance = fsm::FsmInstance::create(subsystem->_data);
    subsystem->_instance->changeState(subsystem->_state_uninitialized);

    return subsystem;
};

// Audio-specific implementation (pimpl)
struct AudioSubsystemImpl {
    audiodevice_ptr_t _device;
    audio::singularity::synth_ptr_t _synth;
    // All audio-specific data here
};

// Audio subsystem factory function (lev2-specific)
subsystem_ptr_t createAudioSubsystem() {
    auto subsystem = std::make_shared<Subsystem>("audio");

    // Store impl using svar64_t (pimpl pattern)
    auto impl = new AudioSubsystemImpl();
    subsystem->_impl.set<AudioSubsystemImpl*>(impl);

    // Build FSM states
    subsystem->_data = std::make_shared<fsm::FsmData>();
    subsystem->_state_uninitialized = subsystem->_data->createState(nullptr, "UNINITIALIZED");
    subsystem->_state_initializing = subsystem->_data->createState(nullptr, "INITIALIZING");
    subsystem->_state_ready = subsystem->_data->createState(nullptr, "READY");
    subsystem->_state_terminated = subsystem->_data->createState(nullptr, "TERMINATED");

    // Setup transitions
    subsystem->_data->addTransition(subsystem->_state_uninitialized, "START", subsystem->_state_initializing);
    subsystem->_data->addTransition(subsystem->_state_initializing, "INIT_DONE", subsystem->_state_ready);

    // Initialize callback
    subsystem->_state_initializing->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        // Initialize audio subsystem
        impl->_synth = audio::singularity::synth::instance();
        impl->_device = audiodevice_ptr_t(new AudioDevice());
        impl->_device->startup();
        inst->sendEvent("INIT_DONE");
    };

    // Shutdown callback
    subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        if (impl->_device) {
            impl->_device->shutdown();
        }
        delete impl;
    };

    // Create FSM instance
    subsystem->_instance = fsm::FsmInstance::create(subsystem->_data);
    subsystem->_instance->changeState(subsystem->_state_uninitialized);

    return subsystem;
};

} // namespace ork::lev2
```

### 2c. User Subsystem Examples

User-defined subsystems follow the same factory pattern as GPU and Audio:

```cpp
namespace ork {

// Physics-specific implementation (pimpl)
struct PhysicsSubsystemImpl {
    PhysicsEngine* _engine = nullptr;
    std::vector<RigidBody*> _bodies;
    float _timestep = 1.0f / 60.0f;
    // All physics-specific data here
};

// Physics subsystem factory function (user-defined, could be in ork.core or user code)
subsystem_ptr_t createPhysicsSubsystem() {
    auto subsystem = std::make_shared<Subsystem>("physics");

    // Store impl using svar64_t (pimpl pattern)
    auto impl = new PhysicsSubsystemImpl();
    subsystem->_impl.set<PhysicsSubsystemImpl*>(impl);

    // Build FSM states
    subsystem->_data = std::make_shared<fsm::FsmData>();
    subsystem->_state_uninitialized = subsystem->_data->createState(nullptr, "UNINITIALIZED");
    subsystem->_state_initializing = subsystem->_data->createState(nullptr, "INITIALIZING");
    subsystem->_state_ready = subsystem->_data->createState(nullptr, "READY");
    subsystem->_state_terminated = subsystem->_data->createState(nullptr, "TERMINATED");

    // Setup transitions
    subsystem->_data->addTransition(subsystem->_state_uninitialized, "START", subsystem->_state_initializing);
    subsystem->_data->addTransition(subsystem->_state_initializing, "INIT_DONE", subsystem->_state_ready);

    // Initialize callback (runs on UPDATE thread if registered with UPDATE affinity)
    subsystem->_state_initializing->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        impl->_engine = new PhysicsEngine();
        impl->_engine->initialize();
        inst->sendEvent("INIT_DONE");
    };

    // Update callback (called every frame on UPDATE thread)
    subsystem->_state_ready->_onupdate = [impl](fsm::fsminstance_ptr_t inst) {
        impl->_engine->simulate(impl->_timestep);
    };

    // Shutdown callback
    subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        if (impl->_engine) {
            impl->_engine->shutdown();
            delete impl->_engine;
        }
        delete impl;
    };

    // Create FSM instance
    subsystem->_instance = fsm::FsmInstance::create(subsystem->_data);
    subsystem->_instance->changeState(subsystem->_state_uninitialized);

    return subsystem;
}

// Network-specific implementation (pimpl)
struct NetworkSubsystemImpl {
    NetworkManager* _manager = nullptr;
    std::string _server_address;
    int _port = 0;
    bool _is_connected = false;
    // All network-specific data here
};

// Network subsystem factory function (user-defined, thread-safe - ANY thread affinity)
subsystem_ptr_t createNetworkSubsystem() {
    auto subsystem = std::make_shared<Subsystem>("network");

    // Store impl using svar64_t (pimpl pattern)
    auto impl = new NetworkSubsystemImpl();
    subsystem->_impl.set<NetworkSubsystemImpl*>(impl);

    // Build FSM states
    subsystem->_data = std::make_shared<fsm::FsmData>();
    subsystem->_state_uninitialized = subsystem->_data->createState(nullptr, "UNINITIALIZED");
    subsystem->_state_initializing = subsystem->_data->createState(nullptr, "INITIALIZING");
    subsystem->_state_ready = subsystem->_data->createState(nullptr, "READY");
    subsystem->_state_terminated = subsystem->_data->createState(nullptr, "TERMINATED");

    // Setup transitions
    subsystem->_data->addTransition(subsystem->_state_uninitialized, "START", subsystem->_state_initializing);
    subsystem->_data->addTransition(subsystem->_state_initializing, "INIT_DONE", subsystem->_state_ready);

    // Initialize callback (can run on any thread)
    subsystem->_state_initializing->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        impl->_manager = new NetworkManager();
        impl->_server_address = "localhost";
        impl->_port = 8080;
        impl->_is_connected = impl->_manager->connect(impl->_server_address, impl->_port);
        inst->sendEvent("INIT_DONE");
    };

    // Update callback (poll network packets)
    subsystem->_state_ready->_onupdate = [impl](fsm::fsminstance_ptr_t inst) {
        if (impl->_is_connected) {
            impl->_manager->poll();
        }
    };

    // Shutdown callback
    subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        if (impl->_manager) {
            impl->_manager->disconnect();
            delete impl->_manager;
        }
        delete impl;
    };

    // Create FSM instance
    subsystem->_instance = fsm::FsmInstance::create(subsystem->_data);
    subsystem->_instance->changeState(subsystem->_state_uninitialized);

    return subsystem;
}

} // namespace ork
```

### 3. FsmGroup for Coordination (ork.core)

The base `Application` class uses `fsm::FsmGroup` to coordinate all registered subsystems:

```cpp
namespace ork {

class Application {
    // ... public API ...

protected:
    // Subsystem coordination (inherited by lev2::OrkEzApp)
    std::map<std::string, subsystem_reg_ptr_t> _registered_subsystems;
    fsm::fsmgroup_ptr_t _subsystem_group;
    mutable std::mutex _subsystem_mutex;

    // Helpers for wave-based initialization and shutdown
    std::vector<subsystem_reg_ptr_t> _getIndependentSubsystems();
    std::vector<subsystem_reg_ptr_t> _getReadyToInitSubsystems();
    void _initSubsystem(subsystem_reg_ptr_t reg);
    void _shutdownSubsystem(subsystem_reg_ptr_t reg);
    void _buildShutdownWaves(...);
};

} // namespace ork
```

**Inherited by ork.lev2::OrkEzApp:**
- GPU and Audio subsystems automatically registered
- User subsystems added via `registerSubsystem()`
- All coordinated by the same FsmGroup

---

## State Transition Logic

### Initialization Sequence

**Core Application (ork.core):**
```
UNINITIALIZED
    ↓ START_APP event
INITIALIZING
    ↓
APP_INIT (onEnter: setup queues, string pool, execute pre-init ops)
    ↓ onAppInitExtension() hook (derived classes extend here)
    ↓ Init DURING_APP_INIT subsystems (network, databases, core services)
    ↓ APP_READY event
UPDATE_INIT (onEnter: spawn UPDATE thread)
    ↓ Init AFTER_UPDATE_INIT subsystems (game logic, user systems)
    ↓ Predicate: wait for all subsystems READY
    ↓ UPDATE_READY event
RUNNING
    ↓ onUpdate: process queues
    ↓ onUpdateExtension() hook (derived classes extend here)
```

**OrkEzApp Extension (ork.lev2):**

`buildApplicationFsm()` inserts GPU/Audio states between APP_INIT and UPDATE_INIT:

```
APP_INIT (core)
    ↓ Init subsystems with no dependencies (network, physics)
    ↓ APP_READY event
GPU_INIT (EzApp inserts)
    GPU_CONTEXT_CREATE (create window, Vulkan init)
        ↓ CONTEXT_CREATED event
    GPU_RESOURCES_LOAD (core resources + user callback)
        ↓ GPU subsystem ready
        ↓ Init subsystems that depend on "gpu" (rendering systems, UI)
        ↓ GPU_READY event
AUDIO_INIT (EzApp inserts)
    SYNTH_INIT (synth::bringUp(), user callback)
        ↓ SYNTH_READY event
    AUDIO_DEVICE_START (audiodevice->startup())
        ↓ Audio subsystem ready
        ↓ Init subsystems that depend on "audio" (audio systems)
        ↓ AUDIO_READY event
UPDATE_INIT (core)
    ↓ Spawn UPDATE thread
    ↓ UPDATE_READY event
    ↓ Physics subsystem ready (runs on UPDATE thread)
    ↓ Init subsystems that depend on "physics" (game logic, ECS)
    ↓ Predicate: all subsystems READY
RUNNING (core + EzApp rendering)
    ↓ Dynamically registered subsystems init when dependencies ready
```

**Dependency-Driven Init Example:**

```
Subsystems registered:
  Network: no dependencies → inits during APP_INIT
  GPU: (Subsystem) → inits during GPU_INIT
  Physics: no dependencies → inits during UPDATE_INIT (UPDATE thread ready)
  RenderSystem: depends on "gpu" → waits for GPU_READY, then inits
  ECS: depends on "gpu", "physics" → waits for both, then inits
```

**Key Insight:** Use predicated transitions to enforce ordering:

```cpp
// UPDATE_INIT can only transition to RUNNING when subsystems ready
auto pred_all_ready = [](fsm::fsminstance_ptr_t inst) -> bool {
    auto app = inst->userdata.get<OrkEzApp*>();
    return app->_subsystem_group->allInState(app->_gpu_fsm->_state_ready)
        && app->_subsystem_group->allInState(app->_audio_fsm->_state_ready);
};

_fsm_data->addTransition(
    _state_update_init,
    "UPDATE_READY",
    fsm::PredicatedTransition(_state_running, pred_all_ready)
);
```

### Shutdown Sequence

**Core Application (ork.core):**
```
RUNNING
    ↓ EXIT_REQUESTED event (from signalExit())
SHUTTING_DOWN
    ↓
EXIT_REQUESTED (onEnter: queues stop accepting work, signal UPDATE thread to exit)
    ↓ Note: Any enqueue attempts now silently no-op
    ↓ UPDATE_SIGNALED event
JOINING_UPDATE (onEnter: _updateThread.join())
    ↓ UPDATE_JOINED event
DRAINING_QUEUES (onEnter: drain _mainq, _updq, _conq until empty)
    ↓ Process remaining work enqueued before EXIT_REQUESTED
    ↓ QUEUES_DRAINED event
    ↓ onShutdownExtension() hook (derived classes extend here)
FINAL_CLEANUP
    ↓ Shutdown all static subsystems (wave-based parallel)
    ↓ CLEANUP_DONE event
TERMINATED
```

**OrkEzApp Extension (ork.lev2):**

`onShutdownExtension()` hook shuts down GPU/Audio AFTER UPDATE thread joins AND queues drain:

```
EXIT_REQUESTED (core)
    ↓ Disable queues
    ↓ Signal UPDATE thread to exit
JOINING_UPDATE (core)
    ↓ _updateThread.join() (core) ← UPDATE THREAD JOINS FIRST
    ↓ UPDATE_JOINED event
DRAINING_QUEUES (core)
    ↓ Drain all queues (work enqueued before EXIT_REQUESTED)
    ↓ QUEUES_DRAINED event
    ↓ onShutdownExtension() called:
        └─ Shutdown subsystems in reverse dependency order
           ├─ Wave 1: Leaf subsystems (e.g., ECS, RenderSystem)
           ├─ Wave 2: Mid-level (e.g., Physics, Network)
           └─ Wave 3: Root resources (e.g., GPU, Audio)
FINAL_CLEANUP (core)
    ↓ DrawQueue cleanup (lev2-specific if present)
    ↓ Close windows (lev2-specific if present)
TERMINATED
```

**Example Dependency Graph Shutdown:**

```
Dependencies:
  ECS → gpu, physics
  RenderSystem → gpu
  Physics → (none)
  GPU → (none)

Shutdown waves (reverse dependency order):
  Wave 1: ECS, RenderSystem (both depend on GPU)
  Wave 2: Physics, GPU (no dependencies or dependents left)
```

**Critical Ordering Enforced:**

1. **Queues stop accepting work immediately**
   - On entering EXIT_REQUESTED, queues set internal flag
   - Any subsequent enqueue attempts silently no-op (return without adding work)
   - Prevents new work from being added during shutdown

2. **UPDATE thread must join FIRST**
   - UPDATE thread may enqueue work right before noticing shutdown signal
   - Work enqueued before EXIT_REQUESTED is preserved
   - Work enqueued after EXIT_REQUESTED is silently dropped

3. **Queues must drain AFTER UPDATE thread joins**
   - Process only work that was enqueued before EXIT_REQUESTED
   - Drain ensures all legitimate pending work is processed

4. **Dependency-driven subsystem shutdown**
   - Subsystems shut down in **reverse dependency order**
   - Example: ECS depends on GPU → ECS shuts down first, then GPU
   - Example: RenderSystem depends on Physics → RenderSystem shuts down first, then Physics
   - Framework automatically builds shutdown waves from dependency graph
   - Leaf nodes (dependents) shut down before root nodes (dependencies)
   - Core subsystems (GPU, Audio, Physics) shut down after all dependent subsystems

6. **DrawQueue sync in FINAL_CLEANUP**
   - Runs after all subsystems and resources cleaned up

---

## Subsystem Graceful Shutdown Pattern

Some subsystems (such as AssetCatalog) have in-flight asynchronous operations that must complete or be cancelled before shutdown. The standard pattern for such subsystems is:

1. **Signal Shutdown Phase** - Stop accepting new operations, set cancellation flag
2. **Drain Pending Phase** - Wait for existing operations to complete or abort

### Implementation Pattern

```cpp
// In subsystem implementation header
struct MySubsystemImpl {
    std::atomic<bool> _shutdown_requested{false};
    std::atomic<int> _inflight_requests{0};

    void requestShutdown();
    void drainPendingOperations();
};

// Implementation
void MySubsystemImpl::requestShutdown() {
    _shutdown_requested = true;
    // Signal any internal managers to stop accepting new work
}

void MySubsystemImpl::drainPendingOperations() {
    // Wait for in-flight operations to complete
    while (_inflight_requests.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
```

### Subsystem Shutdown Handler

```cpp
// In subsystem's SHUTTING_DOWN state entry
subsystem->_state_shutting_down->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
    logchan->log("MySubsystem shutting down...");

    // Phase 1: Signal shutdown to all operations
    impl->requestShutdown();

    // Phase 2: Wait for in-flight operations to complete
    impl->drainPendingOperations();

    logchan->log("MySubsystem shutdown complete");

    // Phase 3: Signal framework we're done
    inst->sendEvent("TERMINATED");
};
```

### Async Operation Pattern

Operations must check the shutdown flag and track in-flight count:

```cpp
void MySubsystemImpl::asyncOperation(Request* request) {
    // Early exit if shutting down
    if (_shutdown_requested) {
        request->_state = RequestState::FAILED;
        return;
    }

    // Track in-flight request
    _inflight_requests.fetch_add(1);

    opq::concurrentQueue()->enqueue([this, request]() {
        // Check shutdown at start of work
        if (_shutdown_requested) {
            request->_state = RequestState::FAILED;
        } else {
            // Do actual work, checking _shutdown_requested in loops
            while (!work_complete && !_shutdown_requested) {
                // Process...
            }
            request->_state = _shutdown_requested
                ? RequestState::FAILED
                : RequestState::SUCCEEDED;
        }

        // Always decrement counter when done
        _inflight_requests.fetch_sub(1);
    });
}
```

### Blocking Loop Cancellation

All blocking loops should check the shutdown flag:

```cpp
// Download wait loop with cancellation
while (!download_complete && !_shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
if (_shutdown_requested) {
    return nullptr;  // Abort on shutdown
}
```

### Shutdown Pattern Comparison

| Pattern | Use Case | Example Subsystems |
|---------|----------|-------------------|
| **Simple cleanup** | RAII resources, no async work | GPU, Audio, Physics |
| **Graceful drain** | Async operations, network I/O | Catalog, Network, Streaming |
| **Ordered cascade** | Dependent child resources | ECS, GameLogic |

**Benefits of Graceful Drain:**

✅ **No hanging operations** - Shutdown doesn't deadlock waiting for work
✅ **Clean resource release** - All operations complete before state transition
✅ **Framework integration** - Works with HFSM subsystem shutdown ordering
✅ **Reusable pattern** - Can be applied to any subsystem with async work

---

## Event-Driven State Updates

### Processing Loop

Replace current flag polling with HFSM event processing:

```cpp
// Current approach (line 547 in ezapp.cpp):
while (not checkAppState(KAPPSTATEFLAG_JOINING)) {
    // ... update loop ...
}

// HFSM approach:
while (_app_fsm->currentState() != _app_fsm->_state_terminated) {
    _app_fsm->update();  // Process pending events

    auto current = _app_fsm->currentState();
    if (current == _app_fsm->_state_running) {
        // Run update logic
        this->_onUpdate();
    } else if (current == _app_fsm->_state_draining_queues) {
        // Process queues
        _mainq->Process();
        _updq->Process();
        // Check if drained
        if (_mainq->isEmpty() && _updq->isEmpty()) {
            _app_fsm->sendEvent("QUEUES_DRAINED");
        }
    } else if (current == _app_fsm->_state_joining_update) {
        // Waiting for UPDATE thread to finish
        // (join() called in state onEnter callback)
        break;  // Exit loop
    }
}
```

### Thread-Safe Event Sending

All threads can send events safely via lock-free queue:

```cpp
// From UPDATE thread:
_app_fsm->sendEvent("UPDATE_READY");

// From MAIN thread:
_app_fsm->sendEvent("EXIT_REQUESTED");

// From audio callback (or any thread):
_app_fsm->sendEvent("AUDIO_DEVICE_ERROR");
```

### Centralized State Processing

```cpp
void OrkEzApp::_mainThreadLoop() {
    // Start app FSM
    _app_fsm->sendEvent("START_APP");

    while (true) {
        // Process FSM transitions
        _app_fsm->update();

        auto current = _app_fsm->currentState();

        if (current == _app_fsm->_state_terminated) {
            break;  // Exit main loop
        } else if (current == _app_fsm->_state_running) {
            // Normal frame iteration
            _mainWindow->_ctqt->_runloopIter(true);
            _renderSecondaryWindows();
            _cleanupClosedSecondaryWindows();
        } else if (current == _app_fsm->_state_shutting_down) {
            // Coordinate shutdown
            // (FSM substates handle details)
            continue;
        } else if (current == _app_fsm->_state_error) {
            // Handle error state
            _handleError();
            break;
        }
    }

    // Ensure we're in terminated state
    while (_app_fsm->currentState() != _app_fsm->_state_terminated) {
        _app_fsm->update();
    }
}
```

---

## Integration with Existing Code

### Minimal Disruption Strategy

**Phase 1: Add FSM alongside flags (no breaking changes)**

```cpp
struct OrkEzApp {
    // Existing state flags (keep for compatibility)
    std::atomic<uint64_t> _appstate;

    // New FSM (opt-in)
    ezappfsm_ptr_t _app_fsm;

    // Compatibility wrappers
    bool checkAppState(uint64_t flag) const {
        // Map FSM state to flags for legacy code
        auto current = _app_fsm->currentState();
        if (current == _app_fsm->_state_running) {
            return flag & KAPPSTATEFLAG_UPDRUNNING;
        } else if (current == _app_fsm->_state_shutting_down) {
            return flag & KAPPSTATEFLAG_JOINING;
        }
        // ... etc
    }
};
```

**Phase 2: Migrate callbacks to FSM events**

```cpp
// Old callback approach:
ctx->_onGpuInit = [this](Context* ctx) {
    // ... init code ...
};

// New FSM approach:
_state_gpu_init->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    auto ctx = this->_mainWindow->_ctqt;
    // ... init code ...
    inst->sendEvent("GPU_READY");
};
```

**Phase 3: Remove state flags entirely**

Once all code migrated, delete `_appstate` and flag constants.

### Callback Integration: FSM Core + User Extensions

The architecture uses **virtual hooks** (template method pattern) to allow derived classes to extend lifecycle phases:

| Lifecycle Phase | Core Responsibility (ork.core) | Extension Hook | lev2 Extension |
|----------------|-------------------------------|----------------|----------------|
| **APP_INIT** | Setup queues, string pool, pre-init ops | `onAppInitExtension()` | Create window context |
| **UPDATE_INIT** | Spawn UPDATE thread | (none) | N/A |
| **RUNNING** | Process main/update/concurrent queues | `onUpdateExtension()` | Render frame, process UI |
| **JOINING_UPDATE** | Join UPDATE thread | (none) | N/A |
| **DRAINING_QUEUES** | Drain all queues, then call extension hook | `onShutdownExtension()` | Shutdown GPU/Audio (AFTER queues drained) |
| **FINAL_CLEANUP** | Shutdown static subsystems | (none) | Close windows, cleanup DrawQueue |

**For backward compatibility, lev2::OrkEzApp also provides user callbacks:**

| User Callback | When Called | Protected |
|---------------|-------------|-----------|
| `_onAppInit` | During APP_INIT | ✅ |
| `_onGpuInit` | During GPU_RESOURCES_LOAD | ✅ |
| `_onSynthInit` | During SYNTH_INIT | ✅ |
| `_onAudioInit` | During AUDIO_DEVICE_START | ✅ |
| `_onUpdateInit` | During UPDATE_INIT | ✅ |
| `_onUpdate` | During RUNNING | ✅ |
| `_onUpdateExit` | During JOINING_UPDATE | ✅ |
| `_onAudioExit` | During AUDIO_SHUTDOWN | ✅ |
| `_onGpuExit` | During GPU_CLEANUP | ✅ |
| `_onAppExit` | During FINAL_CLEANUP | ✅ |

All user callbacks are **protected** (wrapped in try/catch, exceptions logged)

**Implementation Pattern:**

```cpp
// Example: GPU_RESOURCES_LOAD state
_state_gpu_resources_load->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    // === FSM CORE WORK (always happens) ===
    bool core_ok = true;

    // Create essential GPU resources
    if (!_createDrawQueue()) {
        logchan_ezapp->log_error("Failed to create DrawQueue");
        inst->sendEvent("GPU_INIT_FAILED");
        return;
    }

    // Load core shaders
    if (!_loadCoreShaders()) {
        logchan_ezapp->log_error("Failed to load core shaders");
        inst->sendEvent("GPU_INIT_FAILED");
        return;
    }

    // === USER CALLBACK (optional, protected) ===
    if (_onGpuInit) {
        try {
            _onGpuInit(_mainWindow->_ctqt);
            logchan_ezapp->log("User GPU init callback succeeded");
        } catch (const std::exception& e) {
            logchan_ezapp->log_error("User GPU init failed: %s", e.what());
            // Continue - core resources are loaded
        } catch (...) {
            logchan_ezapp->log_error("User GPU init failed with unknown exception");
        }
    }

    // === VALIDATION ===
    if (!_validateGpuResources()) {
        logchan_ezapp->log_error("GPU resource validation failed");
        inst->sendEvent("GPU_INIT_FAILED");
        return;
    }

    // === FSM GUARANTEES FORWARD PROGRESS ===
    inst->sendEvent("GPU_READY");
};
```

### Python Wrapper Integration

Python bindings are split between core and lev2:

**Core Application Bindings (`ork.core`):**

```python
# In orkengine.core
from orkengine.core import fsm, opq

class Application:
    """Base application with FSM lifecycle (no graphics)."""

    def __init__(self, initdata):
        # C++ ork::Application instance
        pass

    def mainThreadLoop(self):
        """Run main application loop."""
        pass

    def registerSubsystem(self, name, subsystem, is_static=False):
        """Register a subsystem. Set dependencies via subsystem.dependencies before calling."""
        pass

    def unregisterSubsystem(self, name):
        """Unregister a subsystem."""
        pass

    def signalExit(self):
        """Request application shutdown."""
        pass

    @property
    def fsm(self):
        """Get FSM instance."""
        return self._fsm

    @property
    def main_queue(self):
        """Get main operation queue."""
        return self._mainq
```

**lev2 OrkEzApp Bindings (extends core):**

```python
# In orkengine.lev2
from orkengine.core import Application

class OrkEzApp(Application):
    """GUI application (extends core Application with GPU/Audio)."""

    def __init__(self, initdata):
        super().__init__(initdata)
        # Additional lev2-specific setup

    # User callbacks (backward compatible)
    def _onGpuInit(self, ctx):
        """Optional: Extend GPU initialization."""
        pass

    def _onAudioInit(self, device):
        """Optional: Extend audio initialization."""
        pass

    # ... other callbacks ...
```

**ComponentizedApplication Pattern:**

```python
class ComponentizedApplication:
    def __init__(self):
        super().__init__()
        self.ezapp = None  # Either core.Application or lev2.OrkEzApp

    def createEzApp(self):
        """Override to create core or lev2 app."""
        # For GUI apps:
        self.ezapp = lev2.OrkEzApp.create(self)

        # For server apps:
        # self.ezapp = core.Application.create(self)

    def _onStateChange(self, inst):
        """React to FSM state changes."""
        current = inst.currentState.name
        self.notify("app_state_change", state=current)
```

**Python Exception Handling:**

All Python callbacks wrapped at C++/Python boundary (see "Protection Mechanisms" section above)

---

## Error Handling and Recovery

### Error States

```
ERROR (parent state)
├── INIT_FAILED
│   ├── APP_INIT_FAILED
│   ├── GPU_INIT_FAILED
│   └── AUDIO_INIT_FAILED
├── GPU_LOST (runtime device loss)
└── UNRECOVERABLE (fatal error)
```

### Error Transitions

From any state, can transition to ERROR on error event:

```cpp
// Add global error transition from all states
for (auto state : _fsm_data->states()) {
    _fsm_data->addTransition(state, "ERROR", _state_error);
}

// Specific error recovery paths
_fsm_data->addTransition(_state_init_failed, "RETRY", _state_app_init);
_fsm_data->addTransition(_state_gpu_lost, "RECREATE_DEVICE", _state_gpu_init);
_fsm_data->addTransition(_state_error, "EXIT", _state_shutting_down);
```

### Example: GPU Device Lost

```cpp
// In rendering code:
if (vulkan_device_lost) {
    _app_fsm->sendEvent("GPU_LOST");
}

// State machine handles recovery:
_state_gpu_lost->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_ezapp->log("GPU device lost, attempting recovery...");

    // Release all GPU resources
    this->_releaseGpuResources();

    // Try to recreate device
    bool success = this->_recreateGpuDevice();

    if (success) {
        inst->sendEvent("RECREATE_DEVICE");  // Back to GPU_INIT
    } else {
        inst->sendEvent("UNRECOVERABLE");    // Give up
    }
};
```

---

## Subsystem Registration and Coordination

### Overview

Higher-level libraries (ork.ecs, custom game engines, networking libraries, etc.) need to register their own subsystems into the application lifecycle. This must work from both C++ and Python, and integrate seamlessly with the HFSM shutdown coordination.

### Design Principles

1. **Dynamic Lifecycle** - Subsystems can be registered/unregistered anytime (even during RUNNING)
2. **Multiple Instances** - Multiple instances of same subsystem type (e.g., multiple ECS per level)
3. **Automatic coordination** - Registered subsystems automatically added to FsmGroup
4. **Lifecycle integration** - Subsystem FSMs coordinate with main app FSM states
5. **Parallel initialization** - Independent subsystems (GPU, Audio) init concurrently
6. **Dependency tracking** - Subsystems can declare dependencies (e.g., ECS depends on GPU)
7. **Thread-safe operations** - Register/unregister from any thread
8. **Clean API** - Simple registration, framework handles complexity

### Use Cases

**Static Subsystems** (lifetime = app lifetime):
- GPU, Audio, Network - registered before mainThreadLoop(), persist until app exit

**Dynamic Subsystems** (created/destroyed during RUNNING):
- ECS per level - created on level load, destroyed on level unload
- Multiple simultaneous ECS - "ecs_level1", "ecs_level2", etc.
- Physics worlds, streaming systems, temporary services

---

## Subsystem Registration API

### C++ Registration

```cpp
// In OrkEzApp.h

// Subsystem metadata for dependency tracking
struct SubsystemRegistration {
    uint64_t name_hash;                     // CRC hash of subsystem name
    std::string name;                       // "gpu", "audio", "ecs" (for debugging)
    subsystem_ptr_t subsystem;
    bool is_static = false;                 // Static subsystems persist until app exit
    std::atomic<bool> is_initializing{false};
    std::atomic<bool> is_shutting_down{false};

    // Note: Dependencies stored in subsystem->_dependencies map
    // Init/shutdown order is IMPLICIT from dependency graph
};

using subsystem_reg_ptr_t = std::shared_ptr<SubsystemRegistration>;

struct OrkEzApp {
    // ... existing members ...

    // Subsystem registration (can be called anytime, even during RUNNING)
    // Dependencies must be set in subsystem->_dependencies before calling this
    void registerSubsystem(
        subsystem_ptr_t subsystem,
        bool is_static = false                       // Static subsystems persist until app exit
    );

    // Convenience overload for string name (auto-hashes to uint64_t)
    void registerSubsystem(
        const std::string& name,
        subsystem_ptr_t subsystem,
        bool is_static = false
    ) {
        // Set the subsystem name hash
        subsystem->_name_hash = CrcString(name).hashed();
        subsystem->_name = name;
        registerSubsystem(subsystem, is_static);
    }

    // Start subsystem (if registered during RUNNING, auto-starts immediately)
    void startSubsystem(const std::string& name);

    // Stop and unregister subsystem (safe to call during RUNNING)
    void unregisterSubsystem(const std::string& name);

    // Stop subsystem but keep registered (can restart later)
    void stopSubsystem(const std::string& name);

    subsystem_ptr_t findSubsystem(const std::string& name);

    // Query subsystems
    size_t subsystemCount() const;
    std::vector<std::string> subsystemNames() const;
    std::vector<std::string> getDependencies(const std::string& name) const;
    bool isSubsystemRunning(const std::string& name) const;

private:
    std::map<std::string, subsystem_reg_ptr_t> _registered_subsystems;
    fsm::fsmgroup_ptr_t _subsystem_group;
    std::mutex _subsystem_mutex;  // Thread-safe registration/unregistration

    // Helper: find subsystems with no dependencies (can init in parallel)
    std::vector<subsystem_reg_ptr_t> _getIndependentSubsystems();

    // Helper: find subsystems whose dependencies are all ready
    std::vector<subsystem_reg_ptr_t> _getReadyToInitSubsystems();

    // Helper: initialize single subsystem (respects dependencies)
    void _initSubsystem(subsystem_reg_ptr_t reg);

    // Helper: shutdown single subsystem (checks dependents)
    void _shutdownSubsystem(subsystem_reg_ptr_t reg);
};
```

**Registration Pattern:**

```cpp
// In higher-level library (e.g., ork.ecs)
namespace ork::ecs {

// ECS-specific implementation (pimpl)
struct EcsSubsystemImpl {
    simulation_ptr_t _simulation;
    // All ECS-specific data here
};

// ECS subsystem factory function
subsystem_ptr_t createEcsSubsystem() {
    auto subsystem = std::make_shared<Subsystem>("ecs");

    // Store impl using svar64_t (pimpl pattern)
    auto impl = new EcsSubsystemImpl();
    subsystem->_impl.set<EcsSubsystemImpl*>(impl);

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

    // Initialize callback - create ECS simulation
    subsystem->_state_initializing->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        logchan_ecs->log("ECS: Initializing simulation");
        impl->_simulation = Simulation::create();
        inst->sendEvent("INIT_DONE");
    };

    // Shutdown callback - cleanup ECS
    subsystem->_state_shutting_down->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        logchan_ecs->log("ECS: Shutting down simulation");
        if (impl->_simulation) {
            impl->_simulation->stopAllSystems();
            impl->_simulation->clearEntities();
            impl->_simulation = nullptr;
        }
        inst->sendEvent("CLEANUP_DONE");
    };

    // Terminated callback - cleanup impl
    subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        delete impl;
    };

    // Create FSM instance
    subsystem->_instance = fsm::FsmInstance::create(subsystem->_data);
    subsystem->_instance->changeState(subsystem->_state_uninitialized);

    return subsystem;
}

} // namespace ork::ecs
```

**Catalog Subsystem Example (Graceful Drain Pattern):**

The AssetCatalog subsystem demonstrates the graceful drain pattern for subsystems with in-flight async operations:

```cpp
// In ork.core/src/application/subsystem_catalog.cpp

namespace ork {

static logchannel_ptr_t logchan_CATALOG = logger()->configureChannel("SUB_CATALOG", fvec3(0.4, 0.6, 1.0), true);

subsystem_ptr_t createCatalogSubsystem() {
    auto subsystem = std::make_shared<Subsystem>("catalog");

    // INITIALIZING state -> gets/creates global catalog instance
    subsystem->_state_initializing->_onenter = [subsystem](fsm::fsminstance_ptr_t inst) {
        logchan_CATALOG->log("Catalog subsystem initializing...");

        using namespace asset::catalog;

        // Lazy initialization - load manifests from $ORKID_ASSET_MANIFEST_DIRS
        auto catalog = AssetCatalog::globalInstance();

        logchan_CATALOG->log("Catalog subsystem initialized");
        inst->sendEvent("READY");
    };

    // SHUTTING_DOWN state -> graceful shutdown with operation drain
    subsystem->_state_shutting_down->_onenter = [subsystem](fsm::fsminstance_ptr_t inst) {
        logchan_CATALOG->log("Catalog subsystem shutting down...");

        using namespace asset::catalog;

        auto catalog = AssetCatalog::globalInstance();

        // Phase 1: Signal shutdown - stops accepting new requests
        // Sets _shutdown_requested flag, signals download manager
        catalog->requestShutdown();

        // Phase 2: Drain pending operations - waits for in-flight requests
        // Blocks until _inflight_requests counter reaches zero
        catalog->drainPendingOperations();

        logchan_CATALOG->log("Catalog subsystem shutdown complete");

        // All operations complete, signal framework
        inst->sendEvent("TERMINATED");
    };

    return subsystem;
}

} // namespace ork
```

**Key Implementation Details:**

- `CatalogImpl::_shutdown_requested` - Atomic bool checked by all blocking loops
- `CatalogImpl::_inflight_requests` - Atomic counter incremented in `fetchAsync()`, decremented when work completes
- `requestShutdown()` - Sets flag and signals download manager to stop accepting new work
- `drainPendingOperations()` - Spins waiting for counter to reach zero
- Blocking loops in `catalog_impl_get.cpp` and `catalog_dl.cpp` check `_shutdown_requested` for early exit

**User Registration (C++):**

```cpp
// In user application code
#include <ork/lev2/ezapp.h>
#include <ork/ecs/subsystem.h>

class MyApp {
    void setup() {
        // Create EzApp
        _ezapp = lev2::OrkEzApp::create(this);

        // === STATIC SUBSYSTEMS (persist until app exit) ===

        // Register Network subsystem (static, no dependencies)
        auto network_subsystem = createNetworkSubsystem();
        // No dependencies - can init immediately
        _ezapp->registerSubsystem(
            "network",
            network_subsystem,
            true   // Static
        );

        // Register Physics subsystem (static, depends on UPDATE thread)
        auto physics_subsystem = createPhysicsSubsystem();
        // Set up dependencies via _dependencies map
        auto update_subsystem = _ezapp->findSubsystem("update");
        if (update_subsystem) {
            physics_subsystem->_dependencies["update"_crcu] = update_subsystem;
        }
        _ezapp->registerSubsystem(
            "physics",
            physics_subsystem,
            true   // Static
        );

        // Note: GPU and Audio subsystems are registered by EzApp
        // User subsystems can depend on them via _dependencies map

        // Start main loop (static subsystems init based on dependency order)
        _ezapp->mainThreadLoop();
    }

    // === DYNAMIC SUBSYSTEMS (created/destroyed during RUNNING) ===

    void loadLevel(const std::string& level_name) {
        // Create unique name for this level's ECS
        std::string ecs_name = "ecs_" + level_name;

        // Create ECS subsystem
        auto ecs = ork::ecs::createEcsSubsystem();

        // Set up dependencies via _dependencies map
        // ECS depends on GPU and Physics
        auto gpu_subsystem = _ezapp->findSubsystem("gpu");
        auto physics_subsystem = _ezapp->findSubsystem("physics");

        if (gpu_subsystem) {
            ecs->_dependencies["gpu"_crcu] = gpu_subsystem;
        }
        if (physics_subsystem) {
            ecs->_dependencies["physics"_crcu] = physics_subsystem;
        }

        // Register ECS (order is IMPLICIT from dependencies)
        _ezapp->registerSubsystem(
            ecs_name,
            ecs,
            false     // is_static = false (dynamic)
        );

        // Init order: GPU ready → Physics ready → ECS initializes
        // Shutdown order: ECS shuts down → Physics → GPU (reverse)

        // registerSubsystem() detects app is RUNNING, starts immediately
        // Waits for dependencies to be ready, then initializes ECS
    }

    void unloadLevel(const std::string& level_name) {
        std::string ecs_name = "ecs_" + level_name;

        // Shutdown and unregister ECS
        _ezapp->unregisterSubsystem(ecs_name);
        // Gracefully shuts down ECS FSM, removes from coordination
    }

private:
    lev2::ezapp_ptr_t _ezapp;
};
```

### Python Registration

**Python Bindings for Registration:**

```cpp
// In pyext_ezapp.cpp
py::class_<lev2::OrkEzApp, lev2::ezapp_ptr_t>(module, "OrkEzApp")
    // ... existing bindings ...
    .def("registerSubsystem",
         py::overload_cast<const std::string&, subsystem_ptr_t, bool>(
             &lev2::OrkEzApp::registerSubsystem),
         py::arg("name"),
         py::arg("subsystem"),
         py::arg("is_static") = false,
         "Register a subsystem FSM for lifecycle coordination. "
         "Set dependencies via subsystem._dependencies before calling.")
    .def("findSubsystem",
         &lev2::OrkEzApp::findSubsystem,
         py::arg("name"),
         "Find a registered subsystem by name")
    .def("unregisterSubsystem",
         &lev2::OrkEzApp::unregisterSubsystem,
         py::arg("name"),
         "Unregister a subsystem")
    .def("subsystemCount",
         &lev2::OrkEzApp::subsystemCount,
         "Get number of registered subsystems")
    .def("subsystemNames",
         &lev2::OrkEzApp::subsystemNames,
         "Get list of registered subsystem names");

// Bind Subsystem base class (in ork.core)
py::class_<ork::Subsystem, subsystem_ptr_t>(module, "Subsystem")
    .def(py::init<const std::string&>())
    .def("initialize", &ork::Subsystem::initialize)
    .def("shutdown", &ork::Subsystem::shutdown)
    .def("currentState", &ork::Subsystem::currentState)
    .def_readonly("name", &ork::Subsystem::_name)
    .def_readonly("name_hash", &ork::Subsystem::_name_hash)
    .def_readonly("instance", &ork::Subsystem::_instance)
    .def_readonly("data", &ork::Subsystem::_data)
    .def_readwrite("dependencies", &ork::Subsystem::_dependencies)
    .def_readwrite("impl", &ork::Subsystem::_impl)
    .def_readwrite("vars", &ork::Subsystem::_vars);
```

**Python Subsystem Definition:**

```python
# In higher-level library (e.g., orkengine.ecs)
from orkengine import core, fsm

# ECS-specific implementation data class
class EcsSubsystemImpl:
    """Pimpl for ECS subsystem - stores implementation data."""
    def __init__(self):
        self.simulation = None

# ECS subsystem factory function
def create_ecs_subsystem():
    """Create and configure ECS subsystem FSM."""
    # Create subsystem instance (no subclassing)
    subsystem = core.Subsystem("ecs")

    # Store impl using svar64_t (pimpl pattern)
    impl = EcsSubsystemImpl()
    subsystem.impl = impl  # Stored in C++ as svar64_t

    # Build FSM states
    subsystem.data = fsm.FsmData()
    subsystem.state_uninitialized = subsystem.data.createState(None, "UNINITIALIZED")
    subsystem.state_initializing = subsystem.data.createState(None, "INITIALIZING")
    subsystem.state_ready = subsystem.data.createState(None, "READY")
    subsystem.state_shutting_down = subsystem.data.createState(None, "SHUTTING_DOWN")
    subsystem.state_terminated = subsystem.data.createState(None, "TERMINATED")

    # Setup transitions
    subsystem.data.addTransition(subsystem.state_uninitialized, "START", subsystem.state_initializing)
    subsystem.data.addTransition(subsystem.state_initializing, "INIT_DONE", subsystem.state_ready)
    subsystem.data.addTransition(subsystem.state_ready, "SHUTDOWN", subsystem.state_shutting_down)
    subsystem.data.addTransition(subsystem.state_shutting_down, "CLEANUP_DONE", subsystem.state_terminated)

    # Initialization callback
    def on_init(inst):
        print("ECS: Initializing simulation")
        from orkengine.ecs import Simulation
        impl.simulation = Simulation()
        inst.sendEvent("INIT_DONE")

    subsystem.state_initializing.onEnter = on_init

    # Shutdown callback
    def on_shutdown(inst):
        print("ECS: Shutting down simulation")
        if impl.simulation:
            impl.simulation.stop_all_systems()
            impl.simulation.clear_entities()
            impl.simulation = None
        inst.sendEvent("CLEANUP_DONE")

    subsystem.state_shutting_down.onEnter = on_shutdown

    # Create FSM instance
    subsystem.instance = fsm.FsmInstance(subsystem.data)
    subsystem.instance.changeState(subsystem.state_uninitialized)

    return subsystem
```

**User Registration (Python):**

```python
# In user application
from orkengine.core import lev2
from orkengine.ecs import create_ecs_subsystem
from my_game.networking import create_network_subsystem

class MyApp(ComponentizedApplication):
    def __init__(self):
        super().__init__()
        self.level_ecs_instances = {}  # Track dynamic ECS instances

    def createEzApp(self):
        super().createEzApp()

        # === STATIC SUBSYSTEMS (persist until app exit) ===

        # GPU and Audio init in parallel (static)
        gpu_subsystem = create_gpu_subsystem()
        self.ezapp.registerSubsystem("gpu", gpu_subsystem, is_static=True)

        audio_subsystem = create_audio_subsystem()
        self.ezapp.registerSubsystem("audio", audio_subsystem, is_static=True)

        # Network init in parallel (static, no dependencies)
        network_subsystem = create_network_subsystem()
        self.ezapp.registerSubsystem("network", network_subsystem, is_static=True)

    # === DYNAMIC SUBSYSTEMS ===

    def load_level(self, level_name):
        """Load a level - creates a new ECS instance dynamically."""
        ecs_name = f"ecs_{level_name}"

        # Create ECS subsystem
        ecs = create_ecs_subsystem()

        # Set up dependencies via _dependencies map
        # ECS depends on GPU (order is IMPLICIT)
        gpu_subsystem = self.ezapp.findSubsystem("gpu")
        if gpu_subsystem:
            # Use CRC hash for dependency key
            from orkengine.core import CrcString
            ecs.dependencies[CrcString("gpu").hashed()] = gpu_subsystem

        # Register ECS (dynamic subsystem)
        self.ezapp.registerSubsystem(
            ecs_name,
            ecs,
            is_static=False  # Dynamic subsystem
        )

        # Track this ECS
        self.level_ecs_instances[level_name] = ecs_name

        print(f"Level {level_name} ECS initialized")

    def unload_level(self, level_name):
        """Unload a level - destroys the ECS instance."""
        if level_name not in self.level_ecs_instances:
            return

        ecs_name = self.level_ecs_instances[level_name]

        # Shutdown and unregister ECS
        self.ezapp.unregisterSubsystem(ecs_name)

        del self.level_ecs_instances[level_name]
        print(f"Level {level_name} ECS destroyed")

    def _onUpdate(self, updinfo):
        """Example: Load/unload levels during runtime."""
        # User presses key to load level
        if self.key_pressed('1'):
            self.load_level("level1")

        if self.key_pressed('2'):
            self.load_level("level2")

        if self.key_pressed('ESC'):
            # Unload all levels
            for level_name in list(self.level_ecs_instances.keys()):
                self.unload_level(level_name)
```

---

## Subsystem Lifecycle Integration

### Automatic Coordination

When subsystems are registered, the framework automatically:

1. **Adds to FsmGroup** - Subsystem FSM added to `_subsystem_group`
2. **Starts on APP_INIT** - Sends `START` event to all registered subsystems
3. **Waits for Ready** - App FSM waits for all subsystems to reach `READY` before entering `RUNNING`
4. **Coordinated Shutdown** - Broadcasts `SHUTDOWN` event, waits for all to reach `TERMINATED`

**Implementation (in EzApp):**

```cpp
void OrkEzApp::registerSubsystem(
    const std::string& name,
    subsystem_ptr_t subsystem,
    const std::vector<std::string>& dependencies,
    bool is_static
) {
    std::lock_guard<std::mutex> lock(_subsystem_mutex);

    // Check for duplicate name
    if (_registered_subsystems.count(name)) {
        logchan_ezapp->log_error("Subsystem '%s' already registered", name.c_str());
        return;
    }

    // Create registration record
    auto reg = std::make_shared<SubsystemRegistration>();
    reg->name = name;
    reg->subsystem = subsystem;
    reg->dependencies = dependencies;
    reg->is_static = is_static;

    // Store subsystem
    _registered_subsystems[name] = reg;

    // Add to coordination group
    if (!_subsystem_group) {
        _subsystem_group = std::make_shared<fsm::FsmGroup>();
    }
    _subsystem_group->addInstance(subsystem->_instance);

    logchan_ezapp->log("Registered subsystem: %s (dependencies: %zu, static: %d)",
                      name.c_str(), dependencies.size(), is_static);

    // If app is already RUNNING, initialize immediately
    auto app_state = _app_fsm->currentState();
    if (app_state == _app_fsm->_state_running) {
        logchan_ezapp->log("App is RUNNING, starting subsystem immediately: %s", name.c_str());
        _initSubsystem(reg);
    }
    // Otherwise, subsystem will init during APP_INIT state
}

void OrkEzApp::unregisterSubsystem(const std::string& name) {
    std::lock_guard<std::mutex> lock(_subsystem_mutex);

    auto it = _registered_subsystems.find(name);
    if (it == _registered_subsystems.end()) {
        logchan_ezapp->log_warning("Subsystem '%s' not found for unregister", name.c_str());
        return;
    }

    auto reg = it->second;

    // Check if any other subsystem depends on this one
    for (auto& [other_name, other_reg] : _registered_subsystems) {
        if (other_name == name) continue;

        for (const auto& dep : other_reg->dependencies) {
            if (dep == name) {
                logchan_ezapp->log_error(
                    "Cannot unregister '%s': subsystem '%s' depends on it",
                    name.c_str(), other_name.c_str());
                return;
            }
        }
    }

    logchan_ezapp->log("Unregistering subsystem: %s", name.c_str());

    // Shutdown subsystem
    _shutdownSubsystem(reg);

    // Remove from group
    // Note: FsmGroup::removeInstance() would need to be added to FSM API
    // For now, group will just have terminated instance

    // Remove from map
    _registered_subsystems.erase(it);

    logchan_ezapp->log("Subsystem unregistered: %s", name.c_str());
}

// Helper: get subsystems with no dependencies (can init first, in parallel)
std::vector<subsystem_reg_ptr_t> OrkEzApp::_getIndependentSubsystems() {
    std::vector<subsystem_reg_ptr_t> result;
    for (auto& [name, reg] : _registered_subsystems) {
        // Skip if already initialized or shutting down
        auto state = reg->subsystem->currentState();
        if (state != reg->subsystem->_state_uninitialized) continue;
        if (reg->is_shutting_down) continue;

        if (reg->dependencies.empty()) {
            result.push_back(reg);
        }
    }
    return result;
}

// Helper: initialize single subsystem (respects dependencies)
void OrkEzApp::_initSubsystem(subsystem_reg_ptr_t reg) {
    if (reg->is_initializing) {
        logchan_ezapp->log_warning("Subsystem '%s' already initializing", reg->name.c_str());
        return;
    }

    reg->is_initializing = true;

    // Check dependencies are ready
    for (const auto& dep_name : reg->dependencies) {
        auto it = _registered_subsystems.find(dep_name);
        if (it == _registered_subsystems.end()) {
            logchan_ezapp->log_error("Subsystem '%s' has unknown dependency '%s'",
                                    reg->name.c_str(), dep_name.c_str());
            reg->is_initializing = false;
            return;
        }

        auto dep_state = it->second->subsystem->currentState();
        if (dep_state != it->second->subsystem->_state_ready) {
            logchan_ezapp->log_warning("Subsystem '%s' dependency '%s' not ready, deferring",
                                      reg->name.c_str(), dep_name.c_str());
            reg->is_initializing = false;
            return;
        }
    }

    // Initialize in background thread
    std::thread init_thread([reg]() {
        logchan_ezapp->log("Initializing subsystem: %s", reg->name.c_str());

        reg->subsystem->_instance->sendEvent("START");

        // Process until READY or ERROR
        while (true) {
            reg->subsystem->_instance->update();

            auto state = reg->subsystem->currentState();
            if (state == reg->subsystem->_state_ready) {
                logchan_ezapp->log("Subsystem ready: %s", reg->name.c_str());
                break;
            } else if (state == reg->subsystem->_state_error) {
                logchan_ezapp->log_error("Subsystem init failed: %s", reg->name.c_str());
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        reg->is_initializing = false;
    });

    init_thread.detach();  // Run in background
}

// Helper: shutdown single subsystem
void OrkEzApp::_shutdownSubsystem(subsystem_reg_ptr_t reg) {
    if (reg->is_shutting_down) {
        return;
    }

    reg->is_shutting_down = true;

    logchan_ezapp->log("Shutting down subsystem: %s", reg->name.c_str());

    reg->subsystem->_instance->sendEvent("SHUTDOWN");

    // Wait for TERMINATED (synchronous)
    while (reg->subsystem->currentState() != reg->subsystem->_state_terminated) {
        reg->subsystem->_instance->update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    logchan_ezapp->log("Subsystem terminated: %s", reg->name.c_str());
    reg->is_shutting_down = false;
}

// Helper: get subsystems whose dependencies are all ready
std::vector<subsystem_reg_ptr_t> OrkEzApp::_getReadyToInitSubsystems() {
    std::vector<subsystem_reg_ptr_t> result;

    for (auto& [name, reg] : _registered_subsystems) {
        // Skip if already past UNINITIALIZED
        auto current = reg->subsystem->currentState();
        if (current != reg->subsystem->_state_uninitialized) {
            continue;
        }

        // Check if all dependencies are ready
        bool deps_ready = true;
        for (const auto& dep_name : reg->dependencies) {
            auto it = _registered_subsystems.find(dep_name);
            if (it == _registered_subsystems.end()) {
                logchan_ezapp->log_error("Subsystem '%s' has unknown dependency '%s'",
                                        name.c_str(), dep_name.c_str());
                deps_ready = false;
                break;
            }

            auto dep_state = it->second->subsystem->currentState();
            if (dep_state != it->second->subsystem->_state_ready) {
                deps_ready = false;
                break;
            }
        }

        if (deps_ready) {
            result.push_back(reg);
        }
    }

    return result;
}
```

**Integration with App FSM States (Dependency-Driven):**

```cpp
// In APP_INIT state - init subsystems with no dependencies
_state_app_init->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    // Core app initialization
    _initAppCore();

    // User callback (protected)
    if (_onAppInit) {
        try {
            _onAppInit();
        } catch (const std::exception& e) {
            logchan_ezapp->log_error("User app init failed: %s", e.what());
        }
    }

    // Initialize subsystems (dependency-driven waves)
    _initSubsystemsInWaves();

    inst->sendEvent("APP_READY");
};

// Helper: initialize all subsystems in dependency-driven waves
void Application::_initSubsystemsInWaves() {
    logchan_app->log("Initializing subsystems (dependency-driven)");

    // Keep processing waves until all subsystems are initialized
    while (true) {
        auto ready_subsystems = _getReadyToInitSubsystems();

        if (ready_subsystems.empty()) {
            break;  // All done
        }

        logchan_app->log("Initializing %zu subsystems (wave)", ready_subsystems.size());

        // Initialize this wave in parallel
        std::vector<std::future<void>> futures;
        for (auto& reg : ready_subsystems) {
            reg->is_initializing = true;
            futures.push_back(std::async(std::launch::async, [reg]() {
                // Send START event to subsystem FSM
                reg->subsystem->_instance->sendEvent("START");

                // Process until READY or ERROR
                while (true) {
                    reg->subsystem->_instance->update();

                    auto state = reg->subsystem->currentState();
                    if (state == reg->subsystem->_state_ready ||
                        state == reg->subsystem->_state_error) {
                        break;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }));
        }

        // Wait for wave to complete
        for (auto& future : futures) {
            future.wait();
        }
    }

    logchan_app->log("All subsystems initialized");
}

// Helper: get subsystems whose dependencies are all ready
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

        if (all_deps_ready) {
            ready.push_back(reg);
        }
    }

    return ready;
}
```

**Shutdown (Dependency-Driven Reverse Order):**

```cpp
// Shutdown integration - dependency-driven reverse order

// 1. EXIT_REQUESTED - disable queues, signal subsystems
_state_exit_requested->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_app->log("Exit requested, disabling queues");

    // Disable queues (subsequent enqueues will no-op)
    _mainq->disable();
    _updq->disable();
    _conq->disable();

    // Signal UPDATE thread to exit
    _update_thread_running = false;
    inst->sendEvent("UPDATE_SIGNALED");
};

// 2. JOINING_UPDATE - wait for UPDATE thread to terminate
_state_joining_update->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_app->log("Joining UPDATE thread");

    if (_update_thread.joinable()) {
        _update_thread.join();
    }

    inst->sendEvent("UPDATE_JOINED");
};

// 3. DRAINING_QUEUES - process remaining operations
_state_draining_queues->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_app->log("Draining operation queues");

    // Drain queues until empty
    _mainq->drain();
    _updq->drain();
    _conq->drain();

    inst->sendEvent("QUEUES_DRAINED");
};

// 4. SHUTTING_DOWN - shutdown subsystems in reverse dependency order
_state_shutting_down->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_app->log("Shutting down subsystems");

    // Shutdown all subsystems (dependency-driven reverse order)
    _shutdownSubsystemsInWaves();

    // Call derived class shutdown extension (GPU, Audio in lev2)
    onShutdownExtension();

    inst->sendEvent("SUBSYSTEMS_TERMINATED");
};

// Helper: shutdown all subsystems in reverse dependency order
void Application::_shutdownSubsystemsInWaves() {
    logchan_app->log("Shutting down subsystems (dependency-driven reverse order)");

    // Build shutdown waves (reverse of init order)
    std::vector<std::vector<subsystem_reg_ptr_t>> shutdown_waves;
    _buildShutdownWaves(shutdown_waves);

    // Process waves in order (each wave shuts down in parallel)
    for (auto& wave : shutdown_waves) {
        logchan_app->log("Shutting down %zu subsystems (wave)", wave.size());

        std::vector<std::future<void>> futures;
        for (auto& reg : wave) {
            reg->is_shutting_down = true;
            futures.push_back(std::async(std::launch::async, [reg]() {
                // Send SHUTDOWN event to subsystem FSM
                reg->subsystem->_instance->sendEvent("SHUTDOWN");

                // Process until TERMINATED
                while (true) {
                    reg->subsystem->_instance->update();

                    auto state = reg->subsystem->currentState();
                    if (state == reg->subsystem->_state_terminated) {
                        break;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }));
        }

        // Wait for wave to complete
        for (auto& future : futures) {
            future.wait();
        }
    }

    logchan_app->log("All subsystems terminated");
}

// Helper: build shutdown waves (reverse of init order - leaf nodes first)
void Application::_buildShutdownWaves(
    std::vector<std::vector<subsystem_reg_ptr_t>>& waves
) {
    std::lock_guard<std::mutex> lock(_subsystem_mutex);

    std::set<uint64_t> shutdown_scheduled;
    std::map<uint64_t, subsystem_reg_ptr_t> subsystem_map;

    for (auto& [hash, reg] : _registered_subsystems) {
        subsystem_map[hash] = reg;
    }

    // Build waves by reverse topological sort (leaf nodes first)
    while (shutdown_scheduled.size() < _registered_subsystems.size()) {
        std::vector<subsystem_reg_ptr_t> wave;

        // Find subsystems that no other (unscheduled) subsystem depends on (leaf nodes)
        for (auto& [hash, reg] : subsystem_map) {
            if (shutdown_scheduled.count(hash)) continue;  // Already scheduled

            // Check if any non-scheduled subsystem depends on this one
            bool has_dependents = false;
            for (auto& [other_hash, other_reg] : subsystem_map) {
                if (shutdown_scheduled.count(other_hash)) continue;
                if (other_hash == hash) continue;

                // Does other_reg depend on this one?
                if (other_reg->subsystem->_dependencies.count(hash)) {
                    has_dependents = true;
                    break;
                }
            }

            if (!has_dependents) {
                wave.push_back(reg);
            }
        }

        if (wave.empty() && shutdown_scheduled.size() < _registered_subsystems.size()) {
            logchan_app->log_error("Circular dependency detected in subsystem graph during shutdown");
            // Force shutdown remaining subsystems
            for (auto& [hash, reg] : subsystem_map) {
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
```

---

## Subsystem Coordination via FsmGroup

### Pattern: Wait for All Subsystems Ready

```cpp
void OrkEzApp::_waitForSubsystemsReady() {
    // Create group with all subsystem FSMs
    _subsystem_group = std::make_shared<fsm::FsmGroup>();

    if (_gpu_fsm) {
        _subsystem_group->addInstance(_gpu_fsm->_instance);
    }
    if (_audio_fsm) {
        _subsystem_group->addInstance(_audio_fsm->_instance);
    }
    // ... add more subsystems ...

    // Predicate: all subsystems must be in READY state
    auto all_ready = [this]() -> bool {
        bool gpu_ok = !_gpu_fsm || _gpu_fsm->currentState() == _gpu_fsm->_state_ready;
        bool audio_ok = !_audio_fsm || _audio_fsm->currentState() == _audio_fsm->_state_ready;
        return gpu_ok && audio_ok;
    };

    // Use predicate in transition
    _fsm_data->addTransition(
        _state_update_init,
        "ALL_READY",
        fsm::PredicatedTransition(_state_running, all_ready)
    );
}
```

### Pattern: Coordinated Shutdown

```cpp
void OrkEzApp::_shutdownSubsystems() {
    // Broadcast shutdown to all subsystems
    _subsystem_group->broadcastEvent("SHUTDOWN");

    // Wait for all to reach TERMINATED state
    _subsystem_group->waitForAllInState("TERMINATED");

    // Safe to proceed with final cleanup
    _app_fsm->sendEvent("SUBSYSTEMS_TERMINATED");
}
```

### Example: ECS Shutdown

```cpp
// ECS subsystem factory function (uses pimpl pattern)
subsystem_ptr_t createEcsSubsystem() {
    auto subsystem = std::make_shared<Subsystem>("ecs");

    // ECS-specific impl (pimpl)
    struct EcsSubsystemImpl {
        ecs::simulation_ptr_t _simulation;
    };
    auto impl = new EcsSubsystemImpl();
    subsystem->_impl.set<EcsSubsystemImpl*>(impl);

    // Build FSM states
    subsystem->_data = std::make_shared<fsm::FsmData>();
    subsystem->_state_uninitialized = subsystem->_data->createState(nullptr, "UNINITIALIZED");
    subsystem->_state_initializing = subsystem->_data->createState(nullptr, "INITIALIZING");
    subsystem->_state_ready = subsystem->_data->createState(nullptr, "READY");
    subsystem->_state_shutting_down = subsystem->_data->createState(nullptr, "SHUTTING_DOWN");
    subsystem->_state_terminated = subsystem->_data->createState(nullptr, "TERMINATED");

    // Initialization: create ECS world
    subsystem->_state_initializing->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        impl->_simulation = ecs::Simulation::create();
        inst->sendEvent("INIT_DONE");
    };

    // Shutdown: stop all systems, cleanup entities
    subsystem->_state_shutting_down->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        impl->_simulation->stopAllSystems();
        impl->_simulation->clearEntities();
        inst->sendEvent("CLEANUP_DONE");
    };

    // Terminated: cleanup impl
    subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t inst) {
        delete impl;
    };

    // Setup transitions
    subsystem->_data->addTransition(subsystem->_state_uninitialized, "START", subsystem->_state_initializing);
    subsystem->_data->addTransition(subsystem->_state_initializing, "INIT_DONE", subsystem->_state_ready);
    subsystem->_data->addTransition(subsystem->_state_ready, "SHUTDOWN", subsystem->_state_shutting_down);
    subsystem->_data->addTransition(subsystem->_state_shutting_down, "CLEANUP_DONE", subsystem->_state_terminated);

    // Create FSM instance
    subsystem->_instance = fsm::FsmInstance::create(subsystem->_data);
    subsystem->_instance->changeState(subsystem->_state_uninitialized);

    return subsystem;
}

// In OrkEzApp initialization:
auto ecs_subsystem = createEcsSubsystem();
_subsystem_group->addInstance(ecs_subsystem->_instance);
```

---

## Threading Model with HFSM

### Single Update Call Per Thread

Each thread processes its FSM instance:

```cpp
// MAIN thread loop:
void OrkEzApp::_mainThreadLoop() {
    while (true) {
        _app_fsm->update();  // Process main FSM

        auto state = _app_fsm->currentState();
        if (state == _app_fsm->_state_terminated) break;

        // Run main thread work based on state
        if (state == _app_fsm->_state_running) {
            _mainq->Process();
            _renderFrame();
        }
    }
}

// UPDATE thread loop:
void OrkEzApp::_updateThreadLoop() {
    while (true) {
        // No direct FSM update - responds to state changes via events

        // Check main FSM state (thread-safe read)
        auto main_state = _app_fsm->currentState();
        if (main_state == _app_fsm->_state_shutting_down) {
            break;  // Exit loop
        }

        if (main_state == _app_fsm->_state_running) {
            _updq->Process();
            _runUpdateLogic();
        }
    }

    // Signal UPDATE thread finished
    _app_fsm->sendEvent("UPDATE_JOINED");
}
```

### Queue Shutdown with FSM

**Disabling Queues on Shutdown:**

```cpp
// In EXIT_REQUESTED state - stop accepting new work
_state_exit_requested->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_app->log("Application shutdown requested");

    // Disable all queues - subsequent enqueues will no-op
    _mainq->disable();
    _updq->disable();
    _conq->disable();

    // Signal UPDATE thread to exit
    _update_thread_running = false;

    inst->sendEvent("UPDATE_SIGNALED");
};
```

**Queue Behavior After Disable:**

```cpp
// In opq::OperationQueue::enqueue()
bool OperationQueue::enqueue(operation_t op) {
    if (_disabled) {
        // Silently no-op - don't add work during shutdown
        return false;
    }

    // Normal enqueue logic
    _queue.push(op);
    return true;
}
```

**Benefits:**
- No crashes from enqueuing to shutdown queues
- No need for complex synchronization in application code
- Clear, deterministic cutoff point for new work

### Queue Draining with FSM

```cpp
_state_draining_queues->_onenter = [this](fsm::fsminstance_ptr_t inst) {
    logchan_app->log("Draining queues...");

    // Process all remaining work (enqueued before EXIT_REQUESTED)
    while (true) {
        bool main_empty = !_mainq->Process();
        bool upd_empty = !_updq->Process();
        bool con_empty = !_conq->Process();

        if (main_empty && upd_empty && con_empty) {
            // All drained, transition
            logchan_app->log("All queues drained");
            inst->sendEvent("QUEUES_DRAINED");
            break;
        }

        // Keep processing
        sched_yield();
    }
};
```

---

## Debugging and Visualization

### DOT Graph Generation

```cpp
void OrkEzApp::debugPrintState() {
    // Generate DOT graph with current state highlighted
    std::string dot = _app_fsm->generateDot();

    // Write to file
    file::Path dot_path = "temp://app_state.dot";
    FILE* f = fopen(dot_path.c_str(), "w");
    fprintf(f, "%s", dot.c_str());
    fclose(f);

    // Convert to PNG
    system("dot -Tpng temp://app_state.dot -o temp://app_state.png");

    logchan_ezapp->log("State diagram saved to temp://app_state.png");
}

// Call on error or user request:
if (error_occurred) {
    debugPrintState();
}
```

### State Change Logging

```cpp
// Add callback to every state
for (auto state : _fsm_data->states()) {
    auto lambda_state = std::dynamic_pointer_cast<fsm::LambdaState>(state);
    if (lambda_state) {
        // Wrap existing onEnter
        auto prev_enter = lambda_state->_onenter;
        lambda_state->_onenter = [prev_enter, state](fsm::fsminstance_ptr_t inst) {
            logchan_ezapp->log("FSM: Entering state %s", state->_name.c_str());
            if (prev_enter) {
                prev_enter(inst);
            }
        };
    }
}
```

### Python Introspection

```python
# From Python code:
app = MyApp()
app.createEzApp()

# Check current state
print(f"App state: {app.app_fsm.currentState.name}")

# Wait for specific state
while app.app_fsm.currentState.name != "RUNNING":
    app.app_fsm.update()
    time.sleep(0.01)

# Generate visualization
dot = app.app_fsm.generateDot()
with open("app_state.dot", "w") as f:
    f.write(dot)
```

---

## Implementation Plan

**See [apphfsm_core_layering.md](./apphfsm_core_layering.md) for the detailed implementation strategy.**

This section provides a high-level overview. The full plan includes core/lev2 separation.

### Phase 0: Preparation (1-2 days)
- [ ] Document current edge cases and failure modes
- [ ] Create test cases for shutdown scenarios
- [ ] Audit subsystem init/shutdown sequences

### Phase 1: Core Application Class (3-5 days)
- [ ] Create `ork.core/inc/ork/application/application.h`
- [ ] Implement `Application` base class with FSM
- [ ] Build core states (UNINITIALIZED, INITIALIZING, RUNNING, SHUTTING_DOWN, TERMINATED)
- [ ] Add `disable()` method to OPQ API (enqueues no-op when disabled)
- [ ] Implement subsystem registration (thread-safe, dynamic)
- [ ] Implement OPQ queue management
- [ ] Implement UPDATE thread management
- [ ] Add virtual hooks (onAppInitExtension, onUpdateExtension, onShutdownExtension)
- [ ] Implement EXIT_REQUESTED state (disable queues, signal UPDATE to exit)
- [ ] Write C++ unit tests for core Application
- [ ] Test with server-only example app
- [ ] Test that enqueue no-ops when queues disabled

### Phase 2: Core Subsystem Base (2-3 days)
- [ ] Create `ork.core/inc/ork/application/subsystem.h`
- [ ] Implement `Subsystem` base class
- [ ] Implement `SubsystemRegistration` struct with dependency tracking
- [ ] Test with example network subsystem (core-only)
- [ ] Add Python bindings for core Application and Subsystem

### Phase 2a: Refactor EzApp to Extend Application (5-7 days)
- [ ] Change `OrkEzApp` to inherit from `ork::Application`
- [ ] Override `buildApplicationFsm()` to insert GPU/Audio states
  - [ ] Add GPU_INIT hierarchy (CONTEXT_CREATE, RESOURCES_LOAD)
  - [ ] Add AUDIO_INIT hierarchy (SYNTH_INIT, DEVICE_START)
- [ ] Override lifecycle hooks
  - [ ] `onAppInitExtension()` - create window context
  - [ ] `onUpdateExtension()` - render frame, process UI
  - [ ] `onShutdownExtension()` - shutdown GPU/Audio
- [ ] Create `GpuSubsystem` (lev2)
  - [ ] Core GPU init (FSM-owned)
  - [ ] User GPU init callback (protected)
  - [ ] GPU state validation
- [ ] Create `AudioSubsystem` (lev2)
  - [ ] Core audio init (FSM-owned)
  - [ ] User audio callbacks (protected)
  - [ ] Audio device validation
- [ ] Test that existing lev2 apps still work
- [ ] Test backward compatibility with current callback API
- [ ] Test exception handling in all callbacks

### Phase 3: Running States (3-4 days)
- [ ] Implement RUNNING state in core Application
  - [ ] Process main/update/concurrent queues
  - [ ] Call onUpdateExtension() hook
- [ ] Override onUpdateExtension() in OrkEzApp
  - [ ] Render frame
  - [ ] Process UI events
  - [ ] Implement FREERUN/LOCKSTEP modes (lev2-specific)
- [ ] Test queue processing during RUNNING state
- [ ] Test switching between freerun and lockstep (lev2)
- [ ] Test core-only apps without rendering

### Phase 4: Shutdown States (7-10 days)
- [ ] Implement JOINING_UPDATE state (join UPDATE thread)
- [ ] Implement DRAINING_QUEUES state with proper drain logic (no arbitrary limits)
  - [ ] Drain all work enqueued before EXIT_REQUESTED
  - [ ] Call onShutdownExtension() hook after queues drained
- [ ] Implement AUDIO_SHUTDOWN state (lev2)
  - [ ] Core audio cleanup (FSM-owned)
  - [ ] User audio cleanup callback (protected)
- [ ] Implement GPU_CLEANUP state (lev2)
  - [ ] Core GPU cleanup (FSM-owned)
  - [ ] User GPU cleanup callback (protected)
- [ ] Implement FINAL_CLEANUP state
  - [ ] Core app cleanup (FSM-owned)
  - [ ] User app cleanup callback (protected)
  - [ ] Shutdown static subsystems (wave-based)
- [ ] Test shutdown sequence thoroughly
  - [ ] Verify UPDATE thread joins before queues drain
  - [ ] Verify queues drain before GPU/Audio shutdown
  - [ ] Test with well-behaved callbacks
  - [ ] Test with callbacks that throw exceptions
  - [ ] Test with callbacks not provided (nullptr)
  - [ ] Test that enqueues during shutdown silently no-op
- [ ] Verify DrawQueue synchronization
- [ ] Test secondary window cleanup
- [ ] Test movie recording shutdown

### Phase 5: Error Handling (3-5 days)
- [ ] Implement ERROR parent state
- [ ] Add error transitions from all states
- [ ] Implement INIT_FAILED recovery
- [ ] Implement GPU_LOST recovery
- [ ] Test error scenarios

### Phase 6: Dynamic Subsystem Lifecycle (7-10 days)

**Note:** Subsystem registration already implemented in Phase 1/2. This phase adds dynamic lifecycle and dependency-driven init/shutdown.

- [x] Enhance `SubsystemRegistration` with dynamic lifecycle support
  - [x] Change `name` from string to uint64_t (CRC hash) - DONE
  - [x] Dependencies stored in `subsystem->_dependencies` map (uint64_t → subsystem_ptr_t) - DONE
  - [x] Add `is_static` flag - DONE
  - [x] Add `is_initializing`, `is_shutting_down` atomics - DONE
- [ ] Enhance Application API for dynamic lifecycle
  - [ ] `stopSubsystem(name)` - shutdown but keep registered
  - [ ] `startSubsystem(name)` - restart stopped subsystem
  - [ ] `isSubsystemRunning(name)` - query state
- [ ] Implement dynamic registration during RUNNING
  - [ ] Detect app state in registerSubsystem()
  - [ ] Auto-start if RUNNING, defer if INITIALIZING
- [ ] Implement dependency-driven subsystem initialization
  - [ ] `_buildInitWaves()` - topological sort of dependency graph
  - [ ] Independent subsystems init in parallel (std::async)
  - [ ] Subsystems with dependencies wait for them to be ready
  - [ ] Wave-based parallel init within each dependency level
- [ ] Implement dependency-driven subsystem shutdown
  - [ ] `_buildShutdownWaves()` - reverse topological sort
  - [ ] Shutdown leaves first (dependents), roots last (dependencies)
  - [ ] Wave-based parallel shutdown within each dependency level
  - [ ] Integrate with onShutdownExtension hook
- [ ] Test subsystem coordination
  - [ ] Test parallel init (GPU + Audio in lev2, or Network + Database in core)
  - [ ] Test dependency ordering (ECS waits for GPU)
  - [ ] Test dynamic registration during RUNNING
  - [ ] Test dynamic unregistration during RUNNING
  - [ ] Test multiple instances (ecs_level1, ecs_level2)
  - [ ] Test shutdown waves
  - [ ] Test circular dependency detection
  - [ ] Test unregister with dependents (should fail)
- [ ] Create example subsystems
  - [ ] Network subsystem (core example)
  - [ ] ECS subsystem (can work with core or lev2)
- [ ] Update Python bindings for dynamic lifecycle
- [ ] Add FsmGroup::removeInstance() to FSM core API (if needed)

### Phase 7: Python Integration (3-4 days)
- [ ] Add Python bindings for core `Application` class
- [ ] Add Python bindings for `Subsystem` base class
- [ ] Add Python bindings for lev2 `OrkEzApp` (extends core bindings)
- [ ] Implement Python exception handling wrappers
  - [ ] Catch `py::error_already_set`
  - [ ] Clear Python errors (don't propagate to C++)
  - [ ] Log exceptions with full traceback
- [ ] Update `ComponentizedApplication` to work with both core and lev2
- [ ] Add state change notifications
- [ ] Test Python introspection (core and lev2)
- [ ] Test Python callbacks that raise exceptions
- [ ] Test Python callbacks that are not overridden
- [ ] Update application_framework_tdd.md with FSM documentation
- [ ] Update apphfsm_core_layering.md with finalized Python API
- [ ] Document user callback contract in Python docstrings

### Phase 8: Migration and Cleanup (5-7 days)
- [ ] Remove old state flag system (`_appstate`, `KAPPSTATEFLAG_*`)
- [ ] Remove flag-based compatibility wrappers
- [ ] Update all existing lev2 applications to use new inheritance
- [ ] Create example server-only app using core Application
- [ ] Performance testing and optimization
  - [ ] Benchmark core-only app overhead
  - [ ] Benchmark lev2 app vs. old system
- [ ] Final documentation pass
  - [ ] Core Application API docs
  - [ ] lev2 extension pattern docs
  - [ ] Migration guide for existing apps

### Phase 9: Validation (3-5 days)
- [ ] Run full test suite
- [ ] Test all edge cases documented in Phase 0
- [ ] Stress test shutdown under load
- [ ] Test all platforms (macOS, Linux, Windows if applicable)
- [ ] Performance benchmarking vs. old system
- [ ] Code review

**Total Estimated Time:** 6-8 weeks (with core/lev2 separation)

---

## Open Questions

1. **FSM Ownership:** Should FSM be in core `Application` or lev2 `OrkEzApp`?
   - **Decision:** Core `Application` owns FSM (one FSM per app instance)
   - **Rationale:** Server-side apps need FSM lifecycle without graphics

2. **Subsystem FSM Granularity:** Should every subsystem have its own FSM?
   - **Decision:** Start with GPU (lev2), Audio (lev2), Network (core example), ECS (either)
   - **Pattern:** Complex subsystems with multi-step init/shutdown get their own FSM

3. **Python Update Loop:** Who calls FSM update in Python apps?
   - **Decision:** C++ `mainThreadLoop()` handles it automatically
   - **Rationale:** FSM update is internal, Python just defines callbacks

4. **State Persistence:** Should FSM state be serializable for crash recovery?
   - **Proposal:** Phase 2 feature, not MVP

5. **Performance:** What is overhead of FSM event queue vs. direct flag checks?
   - **Proposal:** Benchmark in Phase 8, optimize if needed

6. **Headless Mode:** How to handle apps without graphics?
   - **Decision:** Use core `Application` directly (no lev2, no GPU states)
   - **Example:** Server-side apps, daemons, CLI tools

7. **Multiple Windows:** Should each window have its own FSM?
   - **Proposal:** No, main app FSM coordinates all windows

8. **atexit() Handlers:** How to integrate with FSM?
   - **Proposal:** `atexit_app()` sends `EXIT_REQUESTED` event if not already shutting down

9. **Subsystem Registration Timing:** When can subsystems be registered?
   - **Proposal:** Anytime (before mainThreadLoop OR during RUNNING)
   - **Static subsystems** registered before mainThreadLoop, persist until app exit
   - **Dynamic subsystems** registered/unregistered during RUNNING (e.g., ECS per level)

10. **Subsystem Startup Order:** How to control subsystem init order?
   - **Proposal:** Dependency-based with parallel execution for independent subsystems
   - **Phase 1:** Wave-based init (all independent subsystems in parallel, then next wave)
   - **Future:** Priority field for ordering within waves if needed

11. **Subsystem State Access:** Can one subsystem access another's state?
   - **Proposal:** Yes, via `app.findSubsystem(name)->currentState()`
   - **Best Practice:** Use events/callbacks, not direct state polling

12. **Multiple Instances of Same Type:** How to handle multiple ECS instances?
   - **Proposal:** Unique names required - use naming convention: "ecs_level1", "ecs_level2"
   - **Responsibility:** User ensures unique names (framework just checks for duplicates)

13. **Dynamic Subsystem Dependencies:** What if dependency unregistered while subsystem running?
   - **Proposal:** Phase 1 - Forbid unregistering if anything depends on it
   - **Future:** Optional "soft dependencies" that degrade gracefully if missing

---

## Success Criteria

✅ **No Race Conditions:** All shutdown sequences validated with ThreadSanitizer

✅ **Deterministic Order:** Every test run produces identical shutdown log sequence

✅ **Clean Resource Cleanup:** Valgrind reports zero leaks in all shutdown paths

✅ **Error Recovery:** GPU device lost and audio failure paths tested and working

✅ **Callback Resilience:** System survives user callbacks that throw exceptions or hang

✅ **Forward Progress Guarantee:** System never hangs due to user code failures

✅ **Graceful Degradation:** Core functionality preserved even when user callbacks fail

✅ **Performance:** No measurable performance regression vs. flag-based system

✅ **Python Integration:** Full FSM visibility from Python, state change callbacks work

✅ **Python Exception Handling:** Python exceptions caught and logged, don't crash app

✅ **Documentation:** All states, transitions, callbacks, and contracts documented with examples

✅ **Test Coverage:** 100% of states and transitions covered by unit tests

✅ **User Callback Tests:** All callbacks tested with: normal operation, exceptions, nullptr

---

## References

- **[Core Layering Architecture](./apphfsm_core_layering.md)** - ork.core vs ork.lev2 separation
- [HFSM TDD](./ork.dox/core/hfsm_tdd.md) - FSM implementation details
- [Application Framework TDD](./ork.dox/core/application_framework_tdd.md) - ComponentizedApplication pattern
- [Session Notes](./ork.data/misc/session_notes.md) - Coding patterns and conventions
- [Current EzApp Implementation](./ork.lev2/src/ezapp.cpp) - What we're replacing

---

*This document is a living design. It will be updated as we iterate on the design and implementation.*

**Last Updated:** 2026-01-12
**Status:** Design Phase - Core/Lev2 Layering Complete
**Next Steps:** Review layered architecture, begin Phase 1 implementation
**See Also:** [apphfsm_core_layering.md](./apphfsm_core_layering.md) for architectural details
