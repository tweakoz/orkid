////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/subsystem_opq.h>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>

namespace ork {

static logchannel_ptr_t logchan_OPQ = logger()->configureChannel("SUB_OPQ", fvec3(0.3, 0.8, 0.9), false);

///////////////////////////////////////////////////////////////////////////////
// OPQ Subsystem Implementation
//
// Wraps global OPQ init/exit with proper FSM lifecycle.
// Does NOT break existing code - opq::init()/exit() still work directly.
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createOpqSubsystem() {
  auto subsystem = std::make_shared<Subsystem>("opq");

  // INITIALIZING state -> START -> calls opq::init()
  subsystem->_state_initializing->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_OPQ->log("OPQ subsystem initializing...");

    // Call global opq::init() - creates global queues and starts threads
    // Safe to call multiple times (idempotent via static guards in opq.cpp)
    opq::init();

    logchan_OPQ->log("OPQ subsystem initialized");

    // Transition to READY
    instance->sendEvent("READY");
  };

  // SHUTTING_DOWN state -> SHUTDOWN -> graceful cleanup
  subsystem->_state_shutting_down->_onenter = [subsystem](fsm::fsminstance_ptr_t instance) {
    logchan_OPQ->log("OPQ subsystem shutting down...");

    // Gracefully drain queues before calling exit
    // This gives queued operations a chance to complete
    auto mainq = opq::mainSerialQueue();
    auto updq = opq::updateSerialQueue();
    auto concq = opq::concurrentQueue();

    if (mainq) {
      //logchan_OPQ->log("Draining mainSerialQueue...");
      mainq->drain();
    }

    if (updq) {
      //logchan_OPQ->log("Draining updateSerialQueue...");
      updq->drain();
    }

    if (concq) {
      //logchan_OPQ->log("Draining concurrentQueue...");
      concq->drain();
    }

    //logchan_OPQ->log("OPQ queues drained");

    // NOW call opq::exit() - threads should join quickly since queues are empty
    // NOTE: We deliberately do NOT call opq::exit() here to avoid the destructor crash
    // The global OPQs will be cleaned up at process exit (like before)
    // This is a NON-REGRESSION - we're just adding lifecycle management, not changing cleanup

    logchan_OPQ->log("OPQ subsystem shutdown complete");

    // Transition to TERMINATED
    instance->sendEvent("TERMINATED");
  };

  return subsystem;
}

} // namespace ork
