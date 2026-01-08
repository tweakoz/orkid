////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Hierarchical Finite State Machine
//
// FsmData: Shared state machine definition (states, transitions, callbacks)
// FsmInstance: Per-entity runtime state (current state, variables, event queue)
//
// Callbacks receive the instance pointer, allowing shared FsmData with
// instance-specific behavior.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <ork/orktypes.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/varmap.inl>
#include <functional>
#include <ork/util/crc.h>

/////////////////////////////////////////////////////////////////////////////////////

namespace ork::fsm {

/////////////////////////////////////////////////////////////////////////////////////
// Forward declarations
/////////////////////////////////////////////////////////////////////////////////////

struct State;
struct LambdaState;
struct FsmData;
struct FsmInstance;
struct FsmGroup;

using state_ptr_t = std::shared_ptr<State>;
using lambdastate_ptr_t = std::shared_ptr<LambdaState>;
using fsmdata_ptr_t = std::shared_ptr<FsmData>;
using fsminstance_ptr_t = std::shared_ptr<FsmInstance>;
using fsmgroup_ptr_t = std::shared_ptr<FsmGroup>;


/////////////////////////////////////////////////////////////////////////////////////
// Types
/////////////////////////////////////////////////////////////////////////////////////

using fsm_event_t = uint64_t;
using fsmvar_t = svar64_t;
using varmap_t = std::unordered_map<CrcString, fsmvar_t>;

// Callback signatures - receive instance pointer
using state_callback_t = std::function<void(fsminstance_ptr_t)>;
using predicate_callback_t = std::function<bool(fsminstance_ptr_t)>;

/////////////////////////////////////////////////////////////////////////////////////
// Token-based event for efficient string dispatch
// Use "event_name"_crcu to create tokens
/////////////////////////////////////////////////////////////////////////////////////

struct TokenEvent {
  TokenEvent() = default;
  TokenEvent(fsm_event_t token) : _token(token) {}
  fsm_event_t _token = 0;
};

/////////////////////////////////////////////////////////////////////////////////////

struct PredicatedTransition {
  PredicatedTransition() = default;
  PredicatedTransition(state_ptr_t pdest)
      : _destination(pdest) {
    _predicate = [](fsminstance_ptr_t) { return true; };
  }
  PredicatedTransition(state_ptr_t pdest, predicate_callback_t p)
      : _predicate(p)
      , _destination(pdest) {
  }
  predicate_callback_t _predicate;
  state_ptr_t _destination;
  fsm_event_t _event_token = 0;
  std::string _event_name; // For DOT generator (readable label)
};

// Token-based transition map for efficient event dispatch
using token_trans_map_t = std::unordered_map<fsm_event_t, PredicatedTransition>;

/////////////////////////////////////////////////////////////////////////////////////
// DOT graph generation config
/////////////////////////////////////////////////////////////////////////////////////

struct DotConfig {
  std::string graph_name;
  float K = 2.0f;           // Ideal edge length for fdp
  float sep = 25.0f;        // Node separation
  float size_w = 12.0f;     // Output width in inches
  float size_h = 9.0f;      // Output height in inches
  int dpi = 100;            // Resolution
  bool splines = true;      // Curved edges
  bool overlap = false;     // Prevent node overlap
};

/////////////////////////////////////////////////////////////////////////////////////
// State - belongs to FsmData (shared definition)
/////////////////////////////////////////////////////////////////////////////////////

struct State {
  State(FsmData* data, state_ptr_t parent = nullptr, std::string name = "");
  virtual ~State() {}

  virtual void onEnter(fsminstance_ptr_t inst) {}
  virtual void onExit(fsminstance_ptr_t inst) {}
  virtual void onUpdate(fsminstance_ptr_t inst) {}

  size_t index() const { return _index; }

  token_trans_map_t _transitions;
  state_ptr_t _parent;
  std::string _name;
  FsmData* _data = nullptr;
  size_t _index = 0;  // Index in FsmData's state vector
};

/////////////////////////////////////////////////////////////////////////////////////
// LambdaState - state with configurable callbacks
/////////////////////////////////////////////////////////////////////////////////////

struct LambdaState : public State {
  LambdaState(FsmData* data, state_ptr_t parent, std::string name = "");
  void onEnter(fsminstance_ptr_t inst) final;
  void onExit(fsminstance_ptr_t inst) final;
  void onUpdate(fsminstance_ptr_t inst) final;

  state_callback_t _onenter;
  state_callback_t _onexit;
  state_callback_t _onupdate;
};

/////////////////////////////////////////////////////////////////////////////////////
// ChangeStateEvent - internal event for state transitions
/////////////////////////////////////////////////////////////////////////////////////

struct ChangeStateEvent {
  ChangeStateEvent() : _next(nullptr) {}
  state_ptr_t _next;
};

/////////////////////////////////////////////////////////////////////////////////////
// FsmData - Shared state machine definition
//
// Holds states, transitions, and callbacks. Can be shared by multiple instances.
// Immutable after construction (all states/transitions added upfront).
/////////////////////////////////////////////////////////////////////////////////////

struct FsmData {
  FsmData();
  ~FsmData();

  // State creation
  template <typename T> std::shared_ptr<T> newState(state_ptr_t parent, std::string name = "");
  template <typename T> std::shared_ptr<T> newState();
  void addState(state_ptr_t state);

  // Transition setup
  void addTransition(state_ptr_t from, fsm_event_t event, state_ptr_t to);
  void addTransition(state_ptr_t from, fsm_event_t event, const PredicatedTransition& p);
  void addTransition(state_ptr_t from, const std::string& event_name, state_ptr_t to);
  void addTransition(state_ptr_t from, const std::string& event_name, const PredicatedTransition& p);

  // State lookup
  state_ptr_t findState(const std::string& name) const;
  const std::set<state_ptr_t>& states() const { return _stateset; }

  // DOT graph generation
  std::string generateDot(const DotConfig& config = DotConfig(), state_ptr_t current = nullptr) const;

private:
  std::set<state_ptr_t> _stateset;
  std::vector<state_ptr_t> _statesByIndex;
};

/////////////////////////////////////////////////////////////////////////////////////
// FsmInstance - Per-entity runtime state
//
// Each entity gets its own instance, referencing shared FsmData.
// Holds current state, pending events, and instance-specific variables.
/////////////////////////////////////////////////////////////////////////////////////

struct FsmInstance {
  // Factory method
  static fsminstance_ptr_t create(fsmdata_ptr_t data);

  // Update - static because it needs to pass shared_ptr to callbacks
  static void update(fsminstance_ptr_t inst);

  FsmInstance(fsmdata_ptr_t data);
  ~FsmInstance();

  // State changes (member methods - just queue events)
  void changeState(state_ptr_t target);
  void sendEvent(fsm_event_t event);
  void sendEvent(const std::string& event_name);

  // Accessors
  state_ptr_t currentState() const;
  fsmdata_ptr_t data() const { return _data; }
  varmap::varmap_ptr_t vars() const { return _vars; }

  // User data (arbitrary typed storage)
  varmap::var_t _userdata;

  // DOT generation (uses current state from this instance)
  std::string generateDot(const DotConfig& config = DotConfig()) const;

private:
  friend struct FsmGroup;

  static void _performStateChange(fsminstance_ptr_t inst, state_ptr_t to);

  fsmdata_ptr_t _data;
  std::weak_ptr<FsmGroup> _group;
  state_ptr_t _current;
  mutable std::mutex _currentMutex;
  MpMcBoundedQueue<svar16_t, 16> _pendingEvents;
  varmap::varmap_ptr_t _vars;
};

/////////////////////////////////////////////////////////////////////////////////////
// FsmGroup - Coordinate multiple FSM instances
//
// Thread-safe group for broadcasting events and querying state across instances.
// Instances auto-register on construction and auto-unregister on destruction.
/////////////////////////////////////////////////////////////////////////////////////

struct FsmGroup {
  FsmGroup() = default;
  ~FsmGroup() = default;

  // Factory method to create instances in this group
  static fsminstance_ptr_t createInstance(fsmgroup_ptr_t group, fsmdata_ptr_t data);

  // Broadcast event to all instances in the group
  void broadcastEvent(fsm_event_t event);
  void broadcastEvent(const std::string& event_name);

  // Check if all instances are in a specific state
  bool allInState(state_ptr_t state);
  bool allInState(const std::string& state_name);

  // Block until all instances are in a specific state
  void waitForAllInState(state_ptr_t state);
  void waitForAllInState(const std::string& state_name);

  // Number of instances in the group
  size_t count() const;

  // Get current instances (for iteration) - returns copy for thread safety
  std::vector<FsmInstance*> instances() const;

private:
  friend struct FsmInstance;

  void _register(FsmInstance* inst);
  void _unregister(FsmInstance* inst);
  void _notifyStateChange();

  mutable std::mutex _mutex;
  std::condition_variable _cv;
  std::set<FsmInstance*> _instances;
};

/////////////////////////////////////////////////////////////////////////////////////
// Template implementations
/////////////////////////////////////////////////////////////////////////////////////

template <typename T>
std::shared_ptr<T> FsmData::newState(state_ptr_t parent, std::string name) {
  auto state = std::make_shared<T>(this, parent, name);
  addState(state);
  return state;
}

template <typename T>
std::shared_ptr<T> FsmData::newState() {
  auto state = std::make_shared<T>(this);
  addState(state);
  return state;
}

/////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::fsm
