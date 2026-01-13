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
#include <atomic>

namespace ork {

////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////

struct SubsystemFsm;
using subsystemfsm_ptr_t = std::shared_ptr<SubsystemFsm>;

////////////////////////////////////////////////////////////////
// SubsystemFsm - Base class for all subsystems
//
// All subsystems (GPU, Audio, Physics, Network, ECS, etc.) are
// instances of this class. No subclassing needed - use factory
// functions with pimpl pattern instead.
//
// Each subsystem has:
// - Name (string + CRC hash)
// - FSM for lifecycle management
// - Dependencies (pointer map keyed by hash)
// - Implementation storage (svar64_t pimpl)
// - Variable map for properties
////////////////////////////////////////////////////////////////

struct SubsystemFsm {
public:
  SubsystemFsm(const std::string& name);
  ~SubsystemFsm();

  // Called by factory functions to set up FSM states
  void initialize();
  void shutdown();

  // Update the FSM (process events, execute callbacks)
  void update();

  // Accessors
  fsm::state_ptr_t currentState() const;
  const std::string& name() const { return _name; }
  uint64_t nameHash() const { return _name_hash; }

  // Dependency management (proper API instead of exposing map)
  void addDependency(subsystemfsm_ptr_t dep);
  void removeDependency(crcstring_ptr_t token);
  bool hasDependency(crcstring_ptr_t token) const;

  // Orkid patterns - implementation storage
  svar64_t _impl;          // Pimpl - implementation-specific data
  varmap::varmap_ptr_t _vars;  // Variable map for properties

  // Subsystem identity
  uint64_t _name_hash;     // CRC hash of name (e.g., "gpu"_crcu)
  std::string _name;       // "gpu", "audio", "physics" (for debugging)

  // Dependencies - pointer map keyed by hash
  std::unordered_map<uint64_t, subsystemfsm_ptr_t> _dependencies;

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
  subsystemfsm_ptr_t subsystem;
  uint64_t name_hash;
  bool is_static = false;  // Static subsystems init during APP_INIT
  std::atomic<bool> is_initializing{false};
  std::atomic<bool> is_shutting_down{false};
};

using subsystem_reg_ptr_t = std::shared_ptr<SubsystemRegistration>;

} // namespace ork
