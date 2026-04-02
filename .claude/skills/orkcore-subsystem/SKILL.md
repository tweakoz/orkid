---
name: orkcore-subsystem
description: Answer questions about orkid's subsystem initialization framework, dependency-driven init/shutdown, subsystem factories, FSM-based lifecycle, thread affinity, and wave-based parallel initialization. Use when the user asks about subsystems, initialization order, or application startup.
user-invocable: false
---

# Orkid Subsystem Framework Reference

When answering questions about subsystem management in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| Subsystem Base | `ork.core/inc/ork/application/subsystem.h` |
| Subsystem Impl | `ork.core/src/application/subsystem.cpp` |
| OPQ Subsystem | `ork.core/inc/ork/application/subsystem_opq.h` |
| Core Subsystem | `ork.core/inc/ork/application/subsystem_core.h` |
| Catalog Subsystem | `ork.core/inc/ork/application/subsystem_catalog.h` |
| GPU Subsystem | `ork.lev2/inc/ork/lev2/subsystem_gpu.h` |
| Audio Subsystem | `ork.lev2/inc/ork/lev2/subsystem_audio.h` |
| Lev2 Subsystem | `ork.lev2/inc/ork/lev2/subsystem_lev2.h` |
| Application (registry) | `ork.core/inc/ork/application/application.h` |
| Python Bindings | `ork.core/pyext/pyext_application.cpp` |
| Tests | `ork.core/pyext/tests/test_subsystem.py` |
| Ordering Tests | `ork.core/tests/test_subsystem_ordering.py` |

## Architecture Overview

### Subsystem Lifecycle (FSM-Based)
```
UNINITIALIZED → INITIALIZING → READY
                     ↓ (error)
                   ERROR
READY → SHUTTING_DOWN → TERMINATED
```

Each state is an FSM `LambdaState` — subsystem factories override `_onenter` callbacks.

### Factory Functions
```
createOpqSubsystem()      — OPQ task queue lifecycle
createCoreSubsystem()     — Core orkid runtime (depends on OPQ)
createCatalogSubsystem()  — Asset catalog (depends on OPQ)
createGpuSubsystem()      — GPU context (depends on CORE)
createAudioSubsystem()    — Audio device (depends on GPU)
createLev2Subsystem()     — Meta-service: signals when GPU+Audio ready
```

### Dependency Graph
```
OPQ (no deps)
├── CATALOG (depends on OPQ)
└── CORE (depends on OPQ, optionally CATALOG)
    └── GPU (depends on CORE, requires "main" thread)
        └── AUDIO (depends on GPU)
            └── LEV2 (depends on GPU, optionally AUDIO)
```

### Wave-Based Initialization
1. Build waves via topological sort of dependencies
2. Each wave: subsystems with all deps satisfied
3. Within a wave: respect thread affinity
   - `requires_thread = "main"` → serialize on caller thread
   - `requires_thread = ""` → parallel threads
4. Wait for wave completion before next wave
5. Circular dependency detection (asserts)

### Shutdown
- Reverse dependency order (leaves first)
- Same wave-based parallelism
- `_state_shutting_down._onenter` runs cleanup

### Thread Affinity
```cpp
Subsystem("gpu", dependencies={"core"}, children={}, requires_thread="main");
Subsystem("audio", dependencies={"gpu"}, children={}, requires_thread="");
```

### Custom Subsystem Pattern
```cpp
auto sub = std::make_shared<Subsystem>("my_subsystem");
sub->_state_initializing->_onenter = [sub](fsminstance_ptr_t inst) {
  // Custom init logic
  inst->sendEvent("READY");
};
sub->_state_shutting_down->_onenter = [sub](fsminstance_ptr_t inst) {
  // Custom cleanup
  inst->sendEvent("TERMINATED");
};
sub->addDependency(core_subsystem);
app->registerSubsystem(sub);
```

### Registration
```cpp
app->registerSubsystem(subsystem_ptr, is_static=false);
app->getSubsystem("name");       // Lookup by name
app->unregisterSubsystem("name"); // Remove
```

### Python Usage
```python
ezapp = lev2.OrkEzApp.create(app_obj,
  use_subsystems=True,    # Enable HFSM subsystem mode
  enable_audio=True)      # Enable audio subsystem
```

## How to Answer

1. For subsystem lifecycle: read `subsystem.h` for FSM states and `subsystem.cpp` for wave algorithm
2. For specific subsystems: read the corresponding factory header/impl
3. For dependency order: trace `addDependency` calls in factory functions
4. For Python integration: check `test_subsystem.py` examples
