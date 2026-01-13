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
// GPU Subsystem
//
// Wraps GPU lifecycle (context creation, resource loading, cleanup)
// as a subsystem with FSM-based state management.
//
// Dependencies: CORE subsystem (from ork.core)
//
// States:
//   UNINITIALIZED -> INITIALIZING -> READY -> SHUTTING_DOWN -> TERMINATED
//
// The subsystem stores callbacks for user-defined GPU initialization
// and cleanup, which are invoked at the appropriate FSM transitions.
////////////////////////////////////////////////////////////////

// GPU subsystem implementation data (stored in _impl)
struct GpuSubsystemImpl {
  Context* _context = nullptr;
  void_lambda_t _onGpuInit;     // User callback for GPU init
  void_lambda_t _onGpuExit;     // User callback for GPU cleanup
};

// Factory function - creates and configures the GPU subsystem
// The returned subsystem should have its implementation configured
// before being registered with the Application.
subsystem_ptr_t createGpuSubsystem();

// Helper to get the implementation from a GPU subsystem
GpuSubsystemImpl* getGpuSubsystemImpl(subsystem_ptr_t subsystem);

} // namespace ork::lev2
