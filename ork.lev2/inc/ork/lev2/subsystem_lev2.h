////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/application/subsystem.h>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////
// Lev2 Meta-Service Subsystem
//
// This is a meta-service that signals "all lev2 services ready".
// Similar to how the CORE subsystem signals "all core services ready"
// in ork.core, this subsystem signals when GPU and Audio subsystems
// (if enabled) are fully initialized.
//
// Dependencies: GPU subsystem (if enabled), Audio subsystem (if enabled)
//               Falls back to CORE if neither GPU nor Audio enabled
//
// States:
//   UNINITIALIZED -> INITIALIZING -> READY -> SHUTTING_DOWN -> TERMINATED
//
////////////////////////////////////////////////////////////////

// Factory function - creates and configures the lev2 meta-service subsystem
subsystem_ptr_t createLev2Subsystem();

} // namespace ork::lev2
