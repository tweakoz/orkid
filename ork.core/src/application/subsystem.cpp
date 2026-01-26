////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/subsystem.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>
#include <thread>

namespace ork {

using namespace ork::fsm;

static logchannel_ptr_t logchan_SUB = logger()->configureChannel("SUBSYS", fvec3(0.5, 0.8, 0.5), true);

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

  // Wave-based parallel initialization
  // Each wave contains children that can init in parallel (no inter-dependencies)
  // Within a wave:
  //   - Children with _requires_thread="main" init on main thread
  //   - Children with _requires_thread="" can init in parallel threads
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

  int wave_num = 0;
  while (initialized.size() < children_list.size()) {
    // Build this wave - all children that can init now
    std::vector<subsystem_ptr_t> wave_main;    // requires main thread
    std::vector<subsystem_ptr_t> wave_parallel; // can run in parallel

    for (auto& child : children_list) {
      if (initialized.count(child->_name_hash)) continue;
      if (!can_init(child)) continue;

      if (child->_requires_thread == "main") {
        wave_main.push_back(child);
      } else {
        wave_parallel.push_back(child);
      }
    }

    if (wave_main.empty() && wave_parallel.empty()) {
      // No progress - circular dependency
      OrkAssert(false && "Circular dependency among child subsystems");
      break;
    }

    logchan_SUB->log("  init wave %d: %zu main-thread, %zu parallel",
                     wave_num, wave_main.size(), wave_parallel.size());

    // Start parallel threads for non-main-thread children
    std::vector<std::thread> threads;
    for (auto& child : wave_parallel) {
      threads.emplace_back([child]() {
        logchan_SUB->log("    [parallel] initializing: %s", child->_name.c_str());
        child->initialize();
        child->update();
        logchan_SUB->log("    [parallel] initialized: %s", child->_name.c_str());
      });
    }

    // Init main-thread children on current thread
    for (auto& child : wave_main) {
      logchan_SUB->log("    [main] initializing: %s", child->_name.c_str());
      child->initialize();
      child->update();
      logchan_SUB->log("    [main] initialized: %s", child->_name.c_str());
    }

    // Wait for parallel threads to complete
    for (auto& t : threads) {
      t.join();
    }

    // Mark all wave children as initialized
    for (auto& child : wave_main) {
      initialized.insert(child->_name_hash);
    }
    for (auto& child : wave_parallel) {
      initialized.insert(child->_name_hash);
    }

    wave_num++;
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

  // Wave-based parallel shutdown
  // Shutdown in reverse dependency order - children that depend on others shutdown first
  // Within a wave:
  //   - Children with _requires_thread="main" shutdown on main thread
  //   - Children with _requires_thread="" can shutdown in parallel threads
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

  int wave_num = 0;
  while (shutdown_set.size() < children_list.size()) {
    // Build this wave - all children that can shutdown now
    std::vector<subsystem_ptr_t> wave_main;    // requires main thread
    std::vector<subsystem_ptr_t> wave_parallel; // can run in parallel

    for (auto& child : children_list) {
      if (shutdown_set.count(child->_name_hash)) continue;
      if (!can_shutdown(child)) continue;

      if (child->_requires_thread == "main") {
        wave_main.push_back(child);
      } else {
        wave_parallel.push_back(child);
      }
    }

    if (wave_main.empty() && wave_parallel.empty()) {
      // No progress - circular dependency
      OrkAssert(false && "Circular dependency during child shutdown");
      break;
    }

    logchan_SUB->log("  shutdown wave %d: %zu main-thread, %zu parallel",
                     wave_num, wave_main.size(), wave_parallel.size());

    // Start parallel threads for non-main-thread children
    std::vector<std::thread> threads;
    for (auto& child : wave_parallel) {
      threads.emplace_back([child]() {
        logchan_SUB->log("    [parallel] shutting down: %s", child->_name.c_str());
        child->shutdown();
        child->update();
        logchan_SUB->log("    [parallel] shutdown: %s", child->_name.c_str());
      });
    }

    // Shutdown main-thread children on current thread
    for (auto& child : wave_main) {
      logchan_SUB->log("    [main] shutting down: %s", child->_name.c_str());
      child->shutdown();
      child->update();
      logchan_SUB->log("    [main] shutdown: %s", child->_name.c_str());
    }

    // Wait for parallel threads to complete
    for (auto& t : threads) {
      t.join();
    }

    // Mark all wave children as shutdown
    for (auto& child : wave_main) {
      shutdown_set.insert(child->_name_hash);
    }
    for (auto& child : wave_parallel) {
      shutdown_set.insert(child->_name_hash);
    }

    wave_num++;
  }
}

////////////////////////////////////////////////////////////////

} // namespace ork
