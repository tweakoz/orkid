////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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
  enqueueStateChange(nullptr);
  update();
}

void StateMachine::setVar(CrcString key, fsmvar_t var){
  _machineVars[key] = var;
}

///////////////////////////////////////////////////////////////////////////////
fsmvar_t StateMachine::getVar(CrcString key) const{
  fsmvar_t rval;
  auto it = _machineVars.find(key);
  if(it!=_machineVars.end()){
    rval = it->second;
  };
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::enqueueStateChange(state_ptr_t pst) {
  ChangeStateEvent cse;
  cse._next = pst;
  _pendingEvents.push(cse);
}

void StateMachine::enqueueEvent(const svar16_t& v) {
  _pendingEvents.push(v);
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::addState(state_ptr_t pst) {
  _stateset.insert(pst);
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::addTransition(state_ptr_t pfr, State::event_key_t k, state_ptr_t pto) {
  assert(_stateset.find(pfr) != _stateset.end());
  assert(_stateset.find(pto) != _stateset.end());

  auto it = pfr->_transitions.find(k);
  assert(it == pfr->_transitions.end());
  pfr->_transitions.insert(std::make_pair(k, pto));
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::addTransition(state_ptr_t pfr, State::event_key_t k, const PredicatedTransition& p) {
  state_ptr_t pto = p._destination;
  assert(_stateset.find(pfr) != _stateset.end());
  assert(_stateset.find(pto) != _stateset.end());

  auto it = pfr->_transitions.find(k);
  assert(it == pfr->_transitions.end());
  pfr->_transitions.insert(std::make_pair(k, p));
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::_performStateChange(state_ptr_t pto) {
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
      pex->onExit();
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
      pen->onEnter();
  }

  //////////////////////////////////////////////////

  _current = pto;
}

///////////////////////////////////////////////////////////////////////////////

void StateMachine::update() {
  svar16_t ev;
  while (_pendingEvents.try_pop(ev)) {
    if (ev.isA<ChangeStateEvent>()) {
      const auto& cse = ev.get<ChangeStateEvent>();
      _performStateChange(cse._next);
    } else if (_current) {
      auto k  = ev.typeInfo();
      auto it = _current->_transitions.find(k);
      if (it != _current->_transitions.end()) {
        const PredicatedTransition& trans = it->second;
        if (trans._predicate()) {
          auto next = trans._destination;
          _performStateChange(next);
        }
      }
    }
  }
  if (_current)
    _current->onUpdate();
}

///////////////////////////////////////////////////////////////////////////////

LambdaState::LambdaState(StateMachine* machine, state_ptr_t p)
    : State(machine, p) {
}
void LambdaState::onEnter() {
  if (_onenter)
    _onenter();
}
void LambdaState::onExit() {
  if (_onexit)
    _onexit();
}
void LambdaState::onUpdate() {
  if (_onupdate)
    _onupdate();
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::fsm
