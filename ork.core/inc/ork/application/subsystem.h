////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/fsm.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/varmap.inl>
#include <ork/util/crc.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <atomic>

namespace ork {

////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////

struct Subsystem;
using subsystem_ptr_t = std::shared_ptr<Subsystem>;

////////////////////////////////////////////////////////////////
// Subsystem - Base class for all subsystems
//
// All subsystems (GPU, Audio, Physics, Network, ECS, etc.) are
// instances of this class. No subclassing needed - use factory
// functions with pimpl pattern instead.
//
// Each subsystem has:
// - Name (string + CRC hash)
// - FSM for lifecycle management (implementation detail)
// - Dependencies (pointer map keyed by hash)
// - Implementation storage (svar64_t pimpl)
// - Variable map for properties
////////////////////////////////////////////////////////////////

struct Subsystem {
public:
  Subsystem(const std::string& name,
            const std::vector<std::string>& dependencies = {},
            const std::vector<std::string>& children = {},
            const std::string& requires_thread = "");
  ~Subsystem();

  // Called by factory functions to set up FSM states
  void initialize();
  void shutdown();

  // Update the FSM (process events, execute callbacks)
  void update();

  // Accessors
  fsm::state_ptr_t currentState() const;
  const std::string& name() const { return _name; }
  uint64_t nameHash() const { return _name_hash; }
  bool hasParent() const { return !_parent.expired(); }
  subsystem_ptr_t parent() const { return _parent.lock(); }

  // Dependency management (proper API instead of exposing map)
  // Dependencies affect init ordering: deps init first, this inits after
  // Dependencies affect shutdown ordering: this shuts down first, deps shut down after
  void addDependency(subsystem_ptr_t dep);
  void removeDependency(crcstring_ptr_t token);
  bool hasDependency(crcstring_ptr_t token) const;

  // Child management - for meta-services that coordinate other subsystems
  // Children affect init ordering: children init first, this inits after (same as dependency)
  // Children affect shutdown ordering: children shut down first, this shuts down after (opposite of dependency)
  void addChild(subsystem_ptr_t child);
  void removeChild(crcstring_ptr_t token);
  bool hasChild(crcstring_ptr_t token) const;

  // Nested initialization/shutdown - called by parent's FSM callbacks
  // These sort children by inter-dependencies and init/shutdown in correct order
  void initChildren();
  void shutdownChildren();

  // Orkid patterns - implementation storage
  svar64_t _impl;          // Pimpl - implementation-specific data
  varmap::varmap_ptr_t _vars;  // Variable map for properties

  // Subsystem identity
  uint64_t _name_hash;     // CRC hash of name (e.g., "gpu"_crcu)
  std::string _name;       // "gpu", "audio", "physics" (for debugging)

  // Dependencies - pointer map keyed by hash
  // For regular dependencies: dep inits first, dep shuts down after
  std::unordered_map<uint64_t, subsystem_ptr_t> _dependencies;

  // Children - for meta-services
  // Children init first (like dependencies), but children shut down first (opposite of dependencies)
  std::unordered_map<uint64_t, subsystem_ptr_t> _children;

  // Parent - set automatically when added as a child
  // Used by framework to identify root subsystems (those with no parent)
  std::weak_ptr<Subsystem> _parent;

  // Pending names (resolved when graph is built)
  // These are set at construction time and resolved to actual pointers during registration
  std::vector<std::string> _pending_dependencies;
  std::vector<std::string> _pending_children;

  // Thread affinity - which thread this subsystem must init/shutdown on
  // "" = don't care (can run in parallel)
  // "main" = must run on main thread (GPU, GLFW, UI)
  // "audio" = must run on audio thread
  // "update" = must run on update thread
  std::string _requires_thread;

  // FSM access (public for configuration)
  fsm::fsminstance_ptr_t _instance;
  fsm::fsmdata_ptr_t _data;

  // Standard subsystem states (created by factory functions)
  fsm::lambdastate_ptr_t _state_uninitialized;
  fsm::lambdastate_ptr_t _state_initializing;
  fsm::lambdastate_ptr_t _state_ready;
  fsm::lambdastate_ptr_t _state_error;
  fsm::lambdastate_ptr_t _state_shutting_down;
  fsm::lambdastate_ptr_t _state_terminated;

private:
  // Helper to create standard FSM structure
  void _createStandardStates();
};

////////////////////////////////////////////////////////////////
// Subsystem Registration - tracks registration metadata
////////////////////////////////////////////////////////////////

struct SubsystemRegistration {
  subsystem_ptr_t subsystem;
  uint64_t name_hash;
  bool is_static = false;  // Static subsystems init during APP_INIT
  std::atomic<bool> is_initializing{false};
  std::atomic<bool> is_shutting_down{false};
};

using subsystem_reg_ptr_t = std::shared_ptr<SubsystemRegistration>;

} // namespace ork
