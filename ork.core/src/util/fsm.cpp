///////////////////////////////////////////////////////////////////////////////
// MicroOrk (Orkid)
// Copyright 1996-2013, Michael T. Mayers
// Provided under the MIT License (see LICENSE.txt)
///////////////////////////////////////////////////////////////////////////////
// Hierarchical Finite State Machine
///////////////////////////////////////////////////////////////////////////////

#include <ork/util/fsm.h>
#include <ork/kernel/Array.hpp>

namespace ork::fsm {

///////////////////////////////////////////////////////////////////////////////

StateMachine::StateMachine()
    : _current(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

StateMachine::~StateMachine() {
  QueueStateChange(nullptr);
  Update();
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::QueueStateChange(state_ptr_t pst) {
  ChangeStateEvent cse;
  cse._next = pst;
  mPendingEvents.push(cse);
}

void StateMachine::QueueEvent(const svar16_t& v) {
  mPendingEvents.push(v);
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::AddState(state_ptr_t pst) {
  _stateset.insert(pst);
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::AddTransition(state_ptr_t pfr, State::event_key_t k, state_ptr_t pto) {
  assert(_stateset.find(pfr) != _stateset.end());
  assert(_stateset.find(pto) != _stateset.end());

  auto it = pfr->mTransitions.find(k);
  assert(it == pfr->mTransitions.end());
  pfr->mTransitions.insert(std::make_pair(k, pto));
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::AddTransition(state_ptr_t pfr, State::event_key_t k, const PredicatedTransition& p) {
  state_ptr_t pto = p._destination;
  assert(_stateset.find(pfr) != _stateset.end());
  assert(_stateset.find(pto) != _stateset.end());

  auto it = pfr->mTransitions.find(k);
  assert(it == pfr->mTransitions.end());
  pfr->mTransitions.insert(std::make_pair(k, p));
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::PerformStateChange(state_ptr_t pto) {
  assert((pto == nullptr) || (_stateset.find(pto) != _stateset.end()));

  //////////////////////////////////////////////////
  // collect exit handlers from leaf to root
  ///////////////////

  ork::fixedvector<state_ptr_t, 8> exit_vect;
  ork::fixedvector<state_ptr_t, 8> enter_vect;

  auto exit_collect = [&]() {
    state_ptr_t pwalk = _current;
    while (pwalk != nullptr) {
      exit_vect.push_back(pwalk);
      pwalk = pwalk->_parent;
    }
  };

  //////////////////////////////////////////////////
  // collect enter handlers from leaf to root
  ///////////////////

  auto enter_collect = [&]() {
    auto pwalk = pto;
    while (pwalk != nullptr) {
      enter_vect.push_back(pwalk);
      pwalk = pwalk->_parent;
    }
  };

  //////////////////////////////////////////////////

  if (pto == _current) {
    // do nothing
  } else if (nullptr == _current) {
    // nothing to exit
    enter_collect();
  } else if (nullptr == pto) {
    // nothing to enter
    exit_collect();
  }
  /*else if( _current->_parent == pto->_parent )
  {
      // no hierarchy change
      _current->OnExit();
      pto->OnEnter();
  }*/
  else {
    // full hierarchical exit/enter
    exit_collect();
    enter_collect();
  }

  //////////////////////////////////////////////////
  // run collected exit handlers
  //////////////////////////////////////////////////

  for (auto pex : exit_vect) {
    // only run if not present in enter_vect
    bool brun = true;
    for (auto iten : enter_vect)
      if (iten == pex)
        brun = false;
    if (brun)
      pex->OnExit();
  }

  //////////////////////////////////////////////////
  // run collected enter handlers
  //////////////////////////////////////////////////

  for (auto it = enter_vect.rbegin(); it != enter_vect.rend(); it++) {
    state_ptr_t pen = (*it);

    // only run if not present in enter_vect
    bool brun = true;
    for (auto itex : exit_vect)
      if (itex == pen)
        brun = false;
    if (brun)
      pen->OnEnter();
  }

  //////////////////////////////////////////////////

  _current = pto;
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::Update() {
  svar16_t ev;
  while (mPendingEvents.try_pop(ev)) {
    if (ev.IsA<ChangeStateEvent>()) {
      const auto& cse = ev.Get<ChangeStateEvent>();
      PerformStateChange(cse._next);
    } else if (_current) {
      auto k  = ev.GetTypeInfo();
      auto it = _current->mTransitions.find(k);
      if (it != _current->mTransitions.end()) {
        const PredicatedTransition& trans = it->second;
        if (trans.mPredicate()) {
          auto next = trans._destination;
          PerformStateChange(next);
        }
      }
    }
  }
  if (_current)
    _current->OnUpdate();
}

///////////////////////////////////////////////////////////////////////////////

LambdaState::LambdaState(state_ptr_t p)
    : State(p) {
}
void LambdaState::OnEnter() {
  if (_onenter)
    _onenter();
}
void LambdaState::OnExit() {
  if (_onexit)
    _onexit();
}
void LambdaState::OnUpdate() {
  if (_onupdate)
    _onupdate();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::fsm
