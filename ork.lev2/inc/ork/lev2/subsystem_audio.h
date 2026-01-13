////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/application/subsystem.h>
#include <ork/kernel/any.h>
#include <ork/lev2/lev2_types.h>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////
// Audio Subsystem
//
// Wraps Audio lifecycle (device creation, synth init, cleanup)
// as a subsystem with FSM-based state management.
//
// Dependencies: GPU subsystem (audio often needs GPU for visualization)
//
// States:
//   UNINITIALIZED -> INITIALIZING -> READY -> SHUTTING_DOWN -> TERMINATED
//
// The subsystem stores callbacks for user-defined Audio initialization
// and cleanup, which are invoked at the appropriate FSM transitions.
////////////////////////////////////////////////////////////////

// Audio subsystem implementation data (stored in _impl)
struct AudioSubsystemImpl {
  audiodevice_ptr_t _device;
  void_lambda_t _onAudioInit;   // User callback for audio init
  void_lambda_t _onAudioExit;   // User callback for audio cleanup
};

// Factory function - creates and configures the Audio subsystem
// The returned subsystem should have its implementation configured
// before being registered with the Application.
subsystem_ptr_t createAudioSubsystem();

// Helper to get the implementation from an Audio subsystem
AudioSubsystemImpl* getAudioSubsystemImpl(subsystem_ptr_t subsystem);

} // namespace ork::lev2
