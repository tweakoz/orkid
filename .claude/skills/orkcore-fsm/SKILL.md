---
name: orkcore-fsm
description: Answer questions about orkid's hierarchical finite state machine (HFSM), states, transitions, predicated guards, FsmData/FsmInstance, FsmGroup, lambda callbacks, DOT visualization, and Python FSM bindings. Use when the user asks about state machines, FSM, HFSM, or state transitions.
user-invocable: false
---

# Orkid HFSM (Hierarchical Finite State Machine) Reference

When answering questions about state machines in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| FSM Header | `ork.core/inc/ork/util/fsm.h` |
| FSM Implementation | `ork.core/src/util/fsm.cpp` |
| Python Bindings | `ork.core/pyext/pyext_fsm.cpp` |
| Tests | `ork.core/pyext/tests/test_fsm.py` |
| Complex Test | `ork.core/pyext/tests/test_fsm2.py` |
| Visualization Test | `ork.core/pyext/tests/fsmviz_test.py` |

## Architecture Overview

### Two-Part Design
- **FsmData** — shared, immutable state machine definition (states + transitions)
- **FsmInstance** — per-entity runtime (current state, event queue, variables)
- One FsmData can be shared by many FsmInstances

### State Classes
- `State` — base class with virtual `onEnter`/`onExit`/`onUpdate`
- `LambdaState` — concrete state with configurable lambda callbacks (primary Python type)

### Hierarchical Support
States have optional `_parent` pointer forming a tree. Transitions between siblings only exit/enter the leaf states — common ancestors stay active.

Example: transition A1 → A2 (siblings under A):
- Exits: A1 only
- Enters: A2 only
- A stays active (no callbacks)

### Key Classes

```cpp
// Shared definition
struct FsmData {
  template<typename T> shared_ptr<T> newState(state_ptr_t parent, string name);
  void addTransition(state_ptr_t from, string event, state_ptr_t to);
  void addTransition(state_ptr_t from, string event, PredicatedTransition pt);
  state_ptr_t findState(string name);
  string generateDot(DotConfig config);
};

// Per-entity runtime
struct FsmInstance {
  static fsminstance_ptr_t create(fsmdata_ptr_t data);
  static void update(fsminstance_ptr_t inst);
  void sendEvent(string event);
  void changeState(state_ptr_t target);
  void setInitialState(state_ptr_t state);
  state_ptr_t currentState();
  varmap_ptr_t vars();  // Per-instance variables
};

// Multi-instance coordination
struct FsmGroup {
  fsminstance_ptr_t createInstance(fsmdata_ptr_t data);
  void broadcastEvent(string event);
  bool allInState(string state_name);
  void waitForAllInState(string state_name);
};

// Guard condition
struct PredicatedTransition {
  predicate_callback_t _predicate;  // bool(fsminstance_ptr_t)
  state_ptr_t _destination;
};
```

### Python Usage
```python
from orkengine.core import fsm

data = fsm.FsmData()
idle = data.createState(None, "idle")
running = data.createState(None, "running")
data.addTransition(idle, "start", running)
data.addTransition(running, "stop", idle)

idle.onEnter = lambda inst: print("Entering idle")
running.onUpdate = lambda inst: print("Running tick")

inst = fsm.FsmInstance(data)
inst.changeState(idle)
inst.update()

inst.sendEvent("start")
inst.update()
# Now in "running"

# Per-instance variables
inst.vars.counter = 0

# DOT visualization
dot = inst.generateDot()
```

### Event Tokens
Events use 64-bit CRC hashes internally. String-based API converts automatically.

### FsmGroup (Coordination)
```python
group = fsm.FsmGroup()
instances = [group.createInstance(data) for _ in range(5)]
group.broadcastEvent("shutdown")
group.waitForAllInState("terminated")
```

## How to Answer

1. For API: read `fsm.h` for class definitions
2. For Python patterns: check `test_fsm.py` for comprehensive examples
3. For hierarchical behavior: understand parent-child exit/enter traversal
4. For visualization: use `generateDot()` and render with Graphviz
