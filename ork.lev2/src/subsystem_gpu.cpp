////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/subsystem_gpu.h>
#include <ork/util/logger.h>

namespace ork::lev2 {

static logchannel_ptr_t logchan_GPU = logger()->configureChannel("SUB_GPU", fvec3(0.3, 0.8, 0.4), true);

///////////////////////////////////////////////////////////////////////////////
// GPU Subsystem Implementation
//
// This subsystem wraps GPU lifecycle management. The actual GPU context
// creation is done by OrkEzApp - this subsystem just provides a standardized
// lifecycle interface and coordinates with other subsystems.
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createGpuSubsystem() {
  auto subsystem = std::make_shared<Subsystem>("gpu");

  // Create implementation storage
  auto impl = new GpuSubsystemImpl();
  subsystem->_impl.set<GpuSubsystemImpl*>(impl);

  // INITIALIZING state -> calls user GPU init callback
  subsystem->_state_initializing->_onenter = [subsystem, impl](fsm::fsminstance_ptr_t instance) {
    logchan_GPU->log("GPU subsystem initializing...");

    // Call user-defined GPU initialization callback
    if (impl->_onGpuInit) {
      try {
        impl->_onGpuInit();
        logchan_GPU->log("GPU subsystem user init complete");
      } catch (const std::exception& e) {
        logchan_GPU->log("GPU init exception: %s", e.what());
        instance->sendEvent("ERROR");
        return;
      } catch (...) {
        logchan_GPU->log("GPU init unknown exception");
        instance->sendEvent("ERROR");
        return;
      }
    }

    logchan_GPU->log("GPU subsystem initialized");
    instance->sendEvent("READY");
  };

  // SHUTTING_DOWN state -> calls user GPU cleanup callback
  subsystem->_state_shutting_down->_onenter = [subsystem, impl](fsm::fsminstance_ptr_t instance) {
    logchan_GPU->log("GPU subsystem shutting down...");

    // Call user-defined GPU cleanup callback
    if (impl->_onGpuExit) {
      try {
        impl->_onGpuExit();
        logchan_GPU->log("GPU subsystem user cleanup complete");
      } catch (const std::exception& e) {
        logchan_GPU->log("GPU cleanup exception: %s", e.what());
      } catch (...) {
        logchan_GPU->log("GPU cleanup unknown exception");
      }
    }

    logchan_GPU->log("GPU subsystem shutdown complete");
    instance->sendEvent("TERMINATED");
  };

  // TERMINATED state -> delete implementation
  subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t instance) {
    logchan_GPU->log("GPU subsystem terminated - cleaning up impl");
    delete impl;
  };

  return subsystem;
}

///////////////////////////////////////////////////////////////////////////////

GpuSubsystemImpl* getGpuSubsystemImpl(subsystem_ptr_t subsystem) {
  if (!subsystem) return nullptr;
  return subsystem->_impl.get<GpuSubsystemImpl*>();
}

} // namespace ork::lev2
