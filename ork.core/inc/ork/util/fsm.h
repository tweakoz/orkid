///////////////////////////////////////////////////////////////////////////////
// MicroOrk (Orkid)
// Copyright 1996-2013, Michael T. Mayers
// Provided under the MIT License (see LICENSE.txt)
///////////////////////////////////////////////////////////////////////////////
// Hierarchical Finite State Machine
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <set>
#include <ork/orktypes.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/concurrent_queue.h>
#include <functional>

/////////////////////////////////////////////////////////////////////////////////////

namespace ork::fsm {

struct State;
using state_ptr_t = std::shared_ptr<State>;

/////////////////////////////////////////////////////////////////////////////////////

typedef std::function<bool()> bpred_lambda_t;

struct PredicatedTransition {
  PredicatedTransition(state_ptr_t pdest)
      : _destination(pdest) {
    mPredicate = []() { return true; };
  }
  PredicatedTransition(state_ptr_t pdest, bpred_lambda_t p)
      : _destination(pdest)
      , mPredicate(p) {
  }
  bpred_lambda_t mPredicate;
  state_ptr_t _destination;
};

/////////////////////////////////////////////////////////////////////////////////////

template <typename T> const std::type_info* trans_key() {
  return &typeid(T);
}

/////////////////////////////////////////////////////////////////////////////////////

struct State {
  State(state_ptr_t p = nullptr)
      : _parent(p) {
  }

  typedef const std::type_info* event_key_t;
  typedef std::map<event_key_t, PredicatedTransition> trans_map_t;

  virtual void OnEnter() {
  }
  virtual void OnExit() {
  }
  virtual void OnUpdate() {
  }

  trans_map_t mTransitions;
  state_ptr_t _parent;
};

/////////////////////////////////////////////////////////////////////////////////////

struct LambdaState : public State {
  LambdaState(state_ptr_t p);
  void OnEnter();
  void OnExit();
  void OnUpdate();
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

  template <typename T> std::shared_ptr<T> NewState(state_ptr_t par);
  template <typename T> std::shared_ptr<T> NewState();
  void AddState(state_ptr_t pst);
  void AddTransition(state_ptr_t pfr, State::event_key_t k, state_ptr_t pto);
  void AddTransition(state_ptr_t pfr, State::event_key_t k, const PredicatedTransition& p);
  void QueueStateChange(state_ptr_t pst);
  void QueueEvent(const svar16_t& v);

  void Update();
  state_ptr_t currentState() const {
    return _current;
  }

private:
  void PerformStateChange(state_ptr_t pto);

  state_ptr_t _current;
  std::set<state_ptr_t> _stateset;
  MpMcBoundedQueue<svar16_t, 16> mPendingEvents;
};

/////////////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> StateMachine::NewState(state_ptr_t par) {
  auto pst = std::make_shared<T>(par);
  AddState(pst);
  return pst;
}

/////////////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> StateMachine::NewState() {
  auto pst = std::make_shared<T>();
  AddState(pst);
  return pst;
}

/////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::fsm
