////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Hierarchical Finite State Machine
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>
#include <map>
#include <set>
#include <ork/orktypes.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/concurrent_queue.h>
#include <functional>
#include <ork/util/crc.h>

/////////////////////////////////////////////////////////////////////////////////////

namespace ork::fsm {

struct State;
struct LambdaState;
struct StateMachine;
using state_ptr_t = std::shared_ptr<State>;
using lambdastate_ptr_t = std::shared_ptr<LambdaState>;
using statemachine_ptr_t = std::shared_ptr<StateMachine>;
using fsmvar_t = svar64_t;
using varmap_t = std::unordered_map<CrcString,fsmvar_t>;

/////////////////////////////////////////////////////////////////////////////////////

typedef std::function<bool()> bpred_lambda_t;

struct PredicatedTransition {
  PredicatedTransition(state_ptr_t pdest)
      : _destination(pdest) {
    _predicate = [] { return true; };
  }
  PredicatedTransition(state_ptr_t pdest, bpred_lambda_t p)
      : _predicate(p)
      , _destination(pdest) {
  }
  bpred_lambda_t _predicate;
  state_ptr_t _destination;
};

/////////////////////////////////////////////////////////////////////////////////////

template <typename T> const std::type_info* trans_key() {
  return &typeid(T);
}

/////////////////////////////////////////////////////////////////////////////////////

struct State {
  State(StateMachine* machine, state_ptr_t p = nullptr)
      : _parent(p)
      , _machine(machine) {
  }
  virtual ~State() {}

  typedef const std::type_info* event_key_t;
  typedef std::map<event_key_t, PredicatedTransition> trans_map_t;

  virtual void onEnter() {
  }
  virtual void onExit() {
  }
  virtual void onUpdate() {
  }

  trans_map_t _transitions;
  state_ptr_t _parent;
  StateMachine* _machine = nullptr;
};

/////////////////////////////////////////////////////////////////////////////////////

struct LambdaState : public State {
  LambdaState(StateMachine* machine, state_ptr_t p);
  void onEnter() final;
  void onExit() final;
  void onUpdate() final;
  void_lambda_t _onenter;
  void_lambda_t _onexit;
  void_lambda_t _onupdate;
};

/////////////////////////////////////////////////////////////////////////////////////

struct ChangeStateEvent {
  ChangeStateEvent()
      : _next(nullptr) {
  }
  state_ptr_t _next;
};

/////////////////////////////////////////////////////////////////////////////////////

struct StateMachine {
  StateMachine();
  ~StateMachine();

  template <typename T> std::shared_ptr<T> newState(state_ptr_t par);
  template <typename T> std::shared_ptr<T> newState();
  void addState(state_ptr_t pst);
  void addTransition(state_ptr_t pfr, State::event_key_t k, state_ptr_t pto);
  void addTransition(state_ptr_t pfr, State::event_key_t k, const PredicatedTransition& p);
  void enqueueStateChange(state_ptr_t pst);
  void enqueueEvent(const svar16_t& v);

  void update();
  state_ptr_t currentState() const {
    return _current;
  }

  void setVar(CrcString key, fsmvar_t var);
  fsmvar_t getVar(CrcString key) const;

private:
  void _performStateChange(state_ptr_t pto);

  state_ptr_t _current;
  std::set<state_ptr_t> _stateset;
  MpMcBoundedQueue<svar16_t, 16> _pendingEvents;
  varmap_t _machineVars;

};

/////////////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> StateMachine::newState(state_ptr_t par) {
  auto pst = std::make_shared<T>(this, par);
  addState(pst);
  return pst;
}

/////////////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> StateMachine::newState() {
  auto pst = std::make_shared<T>(this);
  addState(pst);
  return pst;
}

/////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::fsm
