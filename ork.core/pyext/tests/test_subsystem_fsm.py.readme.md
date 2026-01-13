# OPQ Subsystem Retrofit - Design Notes

## Problem
The original `core.appinit()` / `core.appexit()` pattern had an OPQ destructor crash:
- `opq::exit()` tried to join threads during cleanup
- Threads weren't properly drained/stopped first
- Result: "mutex lock failed: Invalid argument" crash

## Solution: OPQ Subsystem Wrapper

### Key Design Principles
1. **No Regressions**: Old apps calling `opq::init()`/`exit()` still work
2. **Overlay Functionality**: Wrap existing global OPQ with SubsystemFsm lifecycle
3. **Graceful Shutdown**: Drain queues before cleanup

### Implementation (`opq_subsystem.cpp`)

```cpp
subsystemfsm_ptr_t createOpqSubsystem() {
  auto subsystem = std::make_shared<SubsystemFsm>("opq");

  // INITIALIZING -> calls opq::init() (idempotent)
  subsystem->_state_initializing->_onenter = [...] {
    opq::init();  // Creates global queues, starts threads
  };

  // SHUTTING_DOWN -> drains queues gracefully
  subsystem->_state_shutting_down->_onenter = [...] {
    mainSerialQueue()->drain();
    updateSerialQueue()->drain();
    concurrentQueue()->drain();

    // NOTE: We do NOT call opq::exit() to avoid mutex crash
    // Global OPQs cleaned up at process exit (like before)
  };

  return subsystem;
}
```

### Application Integration

**Constructor:**
```cpp
ork::initModule(_initdata);  // Creates global OPQs via opq::init()

auto opq_subsystem = createOpqSubsystem();
registerSubsystem(opq_subsystem, true);  // Mark as static
opq_subsystem->initialize();  // Transition to READY
opq_subsystem->update();

_mainq = opq::mainSerialQueue();  // Get references to globals
_updq = opq::updateSerialQueue();
_conq = opq::concurrentQueue();
```

**Destructor:**
```cpp
_shutdownSubsystemsInWaves();  // Drains OPQ queues in subsystem
// Do NOT call exitModule() - avoids mutex crash
```

### Benefits
- ✅ Queues are drained before destruction
- ✅ No mutex crash on exit
- ✅ Old code still works (no regression)
- ✅ New apps get proper lifecycle management
- ✅ Pattern established for other subsystems (GPU, Audio, etc.)

### Testing
```python
# Old way (still works):
core.appinit()
# ... use global OPQs ...
core.appexit()

# New way (no crash):
app = core.Application.create()
# ... use app._mainq, app._updq, app._conq ...
# app destructor drains queues gracefully
```

### Future Work
- Retrofit GPU subsystem (depends on OPQ)
- Retrofit Audio subsystem (depends on OPQ)
- Eventually phase out global `opq::init()`/`exit()` calls
