////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/subsystem_lev2.h>
#include <ork/util/logger.h>

namespace ork::lev2 {

static logchannel_ptr_t logchan_LEV2 = logger()->configureChannel("SUB_LEV2", fvec3(0.4, 0.6, 0.8), true);

///////////////////////////////////////////////////////////////////////////////
// Lev2 Meta-Service Subsystem
//
// This subsystem acts as a meta-service that signals when all lev2 subsystems
// (GPU, Audio) are ready. It depends on GPU and/or Audio subsystems if they
// are enabled, otherwise falls back to depending on CORE.
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createLev2Subsystem() {
  auto subsystem = std::make_shared<Subsystem>("lev2");

  // INITIALIZING state -> init children, then transition to READY
  subsystem->_state_initializing->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_LEV2->log("Lev2 meta-service: initializing...");

    // Initialize children (gpu, audio) in dependency order
    // Children are nested under lev2 and initialized here, not by the framework
    subsystem->initChildren();

    logchan_LEV2->log("Lev2 meta-service: all lev2 subsystems ready");
    instance->sendEvent("READY");
  };

  // SHUTTING_DOWN state -> shutdown children, then transition to TERMINATED
  subsystem->_state_shutting_down->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_LEV2->log("Lev2 meta-service: shutting down...");

    // Shutdown children in reverse dependency order
    // Children must shutdown before lev2
    subsystem->shutdownChildren();

    logchan_LEV2->log("Lev2 meta-service: shutdown complete");
    instance->sendEvent("TERMINATED");
  };

  // TERMINATED state -> cleanup complete
  subsystem->_state_terminated->_onenter = [](fsm::fsminstance_ptr_t instance) {
    logchan_LEV2->log("Lev2 meta-service: terminated");
  };

  return subsystem;
}

} // namespace ork::lev2
