////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/subsystem_core.h>
#include <ork/util/logger.h>

namespace ork {

static logchannel_ptr_t logchan_CORE = logger()->configureChannel("CORE_SUBSYSTEM", fvec3(0.9, 0.7, 0.3), true);

///////////////////////////////////////////////////////////////////////////////
// Core Subsystem Implementation
//
// Represents the core orkid runtime being ready.
// This is a coordination point - all core dependencies (OPQ, CATALOG)
// must be READY before CORE transitions to READY.
//
// The actual initialization of core services happens in the dependencies.
// This subsystem just validates that everything is ready and provides
// a single point for higher-level subsystems (GPU, AUDIO) to depend on.
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createCoreSubsystem() {
  auto subsystem = std::make_shared<Subsystem>("core");

  // INITIALIZING state -> validates dependencies and transitions to READY
  subsystem->_state_initializing->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_CORE->log("Core subsystem initializing...");

    // Dependencies (OPQ, optionally CATALOG) are already READY at this point
    // (dependency-driven init ensures this)

    // Any core validation or additional setup can go here

    logchan_CORE->log("Core subsystem initialized - orkid runtime ready");

    // Transition to READY
    instance->sendEvent("READY");
  };

  // SHUTTING_DOWN state -> cleanup coordination
  subsystem->_state_shutting_down->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_CORE->log("Core subsystem shutting down...");

    // Dependencies will be shut down after CORE (reverse order)

    logchan_CORE->log("Core subsystem shutdown complete");

    // Transition to TERMINATED
    instance->sendEvent("TERMINATED");
  };

  return subsystem;
}

} // namespace ork
