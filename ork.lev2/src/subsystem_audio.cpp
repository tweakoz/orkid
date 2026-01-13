////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/subsystem_audio.h>
#include <ork/util/logger.h>

namespace ork::lev2 {

static logchannel_ptr_t logchan_AUDIO = logger()->configureChannel("SUB_AUDIO", fvec3(0.3, 0.4, 0.9), true);

///////////////////////////////////////////////////////////////////////////////
// Audio Subsystem Implementation
//
// This subsystem wraps Audio lifecycle management. The actual audio device
// creation is done by OrkEzApp - this subsystem just provides a standardized
// lifecycle interface and coordinates with other subsystems.
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createAudioSubsystem() {
  auto subsystem = std::make_shared<Subsystem>("audio");

  // Create implementation storage
  auto impl = new AudioSubsystemImpl();
  subsystem->_impl.set<AudioSubsystemImpl*>(impl);

  // INITIALIZING state -> calls user Audio init callback
  subsystem->_state_initializing->_onenter = [subsystem, impl](fsm::fsminstance_ptr_t instance) {
    logchan_AUDIO->log("Audio subsystem initializing...");

    // Call user-defined Audio initialization callback
    if (impl->_onAudioInit) {
      try {
        impl->_onAudioInit();
        logchan_AUDIO->log("Audio subsystem user init complete");
      } catch (const std::exception& e) {
        logchan_AUDIO->log("Audio init exception: %s", e.what());
        instance->sendEvent("ERROR");
        return;
      } catch (...) {
        logchan_AUDIO->log("Audio init unknown exception");
        instance->sendEvent("ERROR");
        return;
      }
    }

    logchan_AUDIO->log("Audio subsystem initialized");
    instance->sendEvent("READY");
  };

  // SHUTTING_DOWN state -> calls user Audio cleanup callback
  subsystem->_state_shutting_down->_onenter = [subsystem, impl](fsm::fsminstance_ptr_t instance) {
    logchan_AUDIO->log("Audio subsystem shutting down...");

    // Call user-defined Audio cleanup callback
    if (impl->_onAudioExit) {
      try {
        impl->_onAudioExit();
        logchan_AUDIO->log("Audio subsystem user cleanup complete");
      } catch (const std::exception& e) {
        logchan_AUDIO->log("Audio cleanup exception: %s", e.what());
      } catch (...) {
        logchan_AUDIO->log("Audio cleanup unknown exception");
      }
    }

    logchan_AUDIO->log("Audio subsystem shutdown complete");
    instance->sendEvent("TERMINATED");
  };

  // TERMINATED state -> delete implementation
  subsystem->_state_terminated->_onenter = [impl](fsm::fsminstance_ptr_t instance) {
    logchan_AUDIO->log("Audio subsystem terminated - cleaning up impl");
    delete impl;
  };

  return subsystem;
}

///////////////////////////////////////////////////////////////////////////////

AudioSubsystemImpl* getAudioSubsystemImpl(subsystem_ptr_t subsystem) {
  if (!subsystem) return nullptr;
  return subsystem->_impl.get<AudioSubsystemImpl*>();
}

} // namespace ork::lev2
