////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/subsystem.h>
#include <ork/kernel/string/deco.inl>

namespace ork {

using namespace ork::fsm;

////////////////////////////////////////////////////////////////

Subsystem::Subsystem(const std::string& name,
                     const std::vector<std::string>& dependencies,
                     const std::vector<std::string>& children,
                     const std::string& requires_thread)
    : _name(name)
    , _name_hash(name.length() > 0 ? CrcString(name.c_str()).hashed() : 0)
    , _vars(std::make_shared<varmap::VarMap>())
    , _pending_dependencies(dependencies)
    , _pending_children(children)
    , _requires_thread(requires_thread) {

  // Create FSM data
  _data = std::make_shared<FsmData>();

  // Create standard states
  _createStandardStates();

  // Create FSM instance AFTER states are created
  _instance = FsmInstance::create(_data);

  // Set initial state directly (no callbacks, no queue - this is the birth state)
  _instance->setInitialState(_state_uninitialized);
}

////////////////////////////////////////////////////////////////

Subsystem::~Subsystem() {
}

////////////////////////////////////////////////////////////////

void Subsystem::_createStandardStates() {
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

  // Allow shutdown from any non-terminal state
  // This handles: subsystems never started, or fatal errors during init requiring abort
  _data->addTransition(_state_uninitialized, "SHUTDOWN", _state_shutting_down);
  _data->addTransition(_state_initializing, "SHUTDOWN", _state_shutting_down);

  // Default initialization behavior: immediately transition to READY
  // Subsystems with custom init logic should override this callback
  _state_initializing->_onenter = [](fsm::fsminstance_ptr_t instance) {
    instance->sendEvent("READY");
  };

  // Default shutdown behavior: immediately transition to TERMINATED
  // Subsystems with custom shutdown logic should override this callback
  _state_shutting_down->_onenter = [](fsm::fsminstance_ptr_t instance) {
    instance->sendEvent("TERMINATED");
  };
}

////////////////////////////////////////////////////////////////

void Subsystem::initialize() {
  _instance->sendEvent("START");
  update();
}

////////////////////////////////////////////////////////////////

void Subsystem::shutdown() {
  _instance->sendEvent("SHUTDOWN");
  update();
}

////////////////////////////////////////////////////////////////

void Subsystem::update() {
  FsmInstance::update(_instance);
}

////////////////////////////////////////////////////////////////

fsm::state_ptr_t Subsystem::currentState() const {
  return _instance->currentState();
}

////////////////////////////////////////////////////////////////

void Subsystem::addDependency(subsystem_ptr_t dep) {
  if (dep) {
    _dependencies[dep->_name_hash] = dep;
  }
}

////////////////////////////////////////////////////////////////

void Subsystem::removeDependency(crcstring_ptr_t token) {
  _dependencies.erase(token->hashed());
}

////////////////////////////////////////////////////////////////

bool Subsystem::hasDependency(crcstring_ptr_t token) const {
  return _dependencies.count(token->hashed()) > 0;
}

////////////////////////////////////////////////////////////////

void Subsystem::addChild(subsystem_ptr_t child) {
  if (child) {
    _children[child->_name_hash] = child;
    // Set child's parent pointer (for framework to identify root subsystems)
    // Note: we need shared_from_this, but Subsystem doesn't inherit from enable_shared_from_this
    // The parent will be set after the subsystem is registered (when we have a shared_ptr to this)
    // For now, we'll set it during initChildren() when we have the context
  }
}

////////////////////////////////////////////////////////////////

void Subsystem::removeChild(crcstring_ptr_t token) {
  _children.erase(token->hashed());
}

////////////////////////////////////////////////////////////////

bool Subsystem::hasChild(crcstring_ptr_t token) const {
  return _children.count(token->hashed()) > 0;
}

////////////////////////////////////////////////////////////////

void Subsystem::initChildren() {
  if (_children.empty()) {
    return;
  }

  // Build list of children
  std::vector<subsystem_ptr_t> children_list;
  for (auto& [hash, child] : _children) {
    children_list.push_back(child);
  }

  // Sort children by inter-dependencies (topological sort)
  // A child can only depend on siblings (other children of same parent)
  std::set<uint64_t> initialized;

  auto can_init = [&](subsystem_ptr_t child) -> bool {
    // Check all dependencies that are siblings
    for (auto& [dep_hash, dep_ptr] : child->_dependencies) {
      // Only check dependencies that are also our children (siblings)
      if (_children.count(dep_hash) > 0) {
        if (initialized.find(dep_hash) == initialized.end()) {
          return false;
        }
      }
    }
    return true;
  };

  while (initialized.size() < children_list.size()) {
    bool progress = false;
    for (auto& child : children_list) {
      if (initialized.count(child->_name_hash)) continue;
      if (!can_init(child)) continue;

      // Initialize this child
      child->initialize();
      child->update();
      initialized.insert(child->_name_hash);
      progress = true;
    }
    if (!progress) {
      // Circular dependency among children - this is a bug
      OrkAssert(false && "Circular dependency among child subsystems");
      break;
    }
  }
}

////////////////////////////////////////////////////////////////

void Subsystem::shutdownChildren() {
  if (_children.empty()) {
    return;
  }

  // Build list of children
  std::vector<subsystem_ptr_t> children_list;
  for (auto& [hash, child] : _children) {
    children_list.push_back(child);
  }

  // Shutdown in reverse dependency order
  // Children that depend on others shutdown first
  std::set<uint64_t> shutdown_set;

  auto can_shutdown = [&](subsystem_ptr_t child) -> bool {
    // Check if any sibling depends on this child
    for (auto& other : children_list) {
      if (other->_name_hash == child->_name_hash) continue;
      if (shutdown_set.count(other->_name_hash)) continue;

      // Does other depend on child?
      if (other->_dependencies.count(child->_name_hash) > 0) {
        return false;  // other depends on child, must shutdown other first
      }
    }
    return true;
  };

  while (shutdown_set.size() < children_list.size()) {
    bool progress = false;
    for (auto& child : children_list) {
      if (shutdown_set.count(child->_name_hash)) continue;
      if (!can_shutdown(child)) continue;

      // Shutdown this child
      child->shutdown();
      child->update();
      shutdown_set.insert(child->_name_hash);
      progress = true;
    }
    if (!progress) {
      // Circular dependency - shouldn't happen if init worked
      OrkAssert(false && "Circular dependency during child shutdown");
      break;
    }
  }
}

////////////////////////////////////////////////////////////////

} // namespace ork
