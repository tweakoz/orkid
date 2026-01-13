////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/application/subsystem_fsm.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// OPQ Subsystem Factory
//
// Creates a subsystem that manages the global OPQ lifecycle.
// Wraps existing opq::init() and opq::exit() with proper FSM states.
//
// Usage in Application:
//   auto opq_subsystem = createOpqSubsystem();
//   registerSubsystem(opq_subsystem);
//
// Old apps can still call opq::init()/exit() directly (no regression).
// New apps using Application get proper lifecycle management.
///////////////////////////////////////////////////////////////////////////////

subsystemfsm_ptr_t createOpqSubsystem();

} // namespace ork
