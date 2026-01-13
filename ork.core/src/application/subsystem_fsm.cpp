////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/subsystem_fsm.h>
#include <ork/kernel/string/deco.inl>

namespace ork {

using namespace ork::fsm;

////////////////////////////////////////////////////////////////

SubsystemFsm::SubsystemFsm(const std::string& name)
    : _name(name)
    , _name_hash(name.length() > 0 ? CrcString(name.c_str()).hashed() : 0)
    , _vars(std::make_shared<varmap::VarMap>()) {

  // Create FSM data
  _data = std::make_shared<FsmData>();

  // Create standard states
  _createStandardStates();

  // Create FSM instance AFTER states are created
  _instance = FsmInstance::create(_data);

  // Set initial state NOW that instance exists
  _instance->changeState(_state_uninitialized);
}

////////////////////////////////////////////////////////////////

SubsystemFsm::~SubsystemFsm() {
}

////////////////////////////////////////////////////////////////

void SubsystemFsm::_createStandardStates() {
  // Create standard subsystem states
  _state_uninitialized = _data->newState<LambdaState>(nullptr, "UNINITIALIZED");
  _state_initializing = _data->newState<LambdaState>(nullptr, "INITIALIZING");
  _state_ready = _data->newState<LambdaState>(nullptr, "READY");
  _state_error = _data->newState<LambdaState>(nullptr, "ERROR");
  _state_shutting_down = _data->newState<LambdaState>(nullptr, "SHUTTING_DOWN");
  _state_terminated = _data->newState<LambdaState>(nullptr, "TERMINATED");

  // Setup default transitions
  _data->addTransition(_state_uninitialized, "START", _state_initializing);
  _data->addTransition(_state_initializing, "READY", _state_ready);
  _data->addTransition(_state_initializing, "ERROR", _state_error);
  _data->addTransition(_state_ready, "SHUTDOWN", _state_shutting_down);
  _data->addTransition(_state_error, "SHUTDOWN", _state_shutting_down);
  _data->addTransition(_state_shutting_down, "TERMINATED", _state_terminated);
}

////////////////////////////////////////////////////////////////

void SubsystemFsm::initialize() {
  _instance->sendEvent("START");
  update();
}

////////////////////////////////////////////////////////////////

void SubsystemFsm::shutdown() {
  _instance->sendEvent("SHUTDOWN");
  update();
}

////////////////////////////////////////////////////////////////

void SubsystemFsm::update() {
  FsmInstance::update(_instance);
}

////////////////////////////////////////////////////////////////

fsm::state_ptr_t SubsystemFsm::currentState() const {
  return _instance->currentState();
}

////////////////////////////////////////////////////////////////

} // namespace ork
