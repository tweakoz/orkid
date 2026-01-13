////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/application/subsystem.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// Core Subsystem Factory
//
// Creates a subsystem representing the core orkid runtime.
// This is a coordination point - when CORE is READY, the basic
// orkid infrastructure is fully initialized.
//
// Dependencies:
//   - OPQ (always)
//   - CATALOG (optional, if _std_asset_catalog is enabled)
//
// Dependents (in ork.lev2):
//   - GPU subsystem depends on CORE
//   - AUDIO subsystem depends on CORE
//
// Usage in Application:
//   auto core_subsystem = createCoreSubsystem();
//   core_subsystem->addDependency(opq_subsystem);
//   if (catalog_subsystem) {
//     core_subsystem->addDependency(catalog_subsystem);
//   }
//   registerSubsystem(core_subsystem);
///////////////////////////////////////////////////////////////////////////////

subsystem_ptr_t createCoreSubsystem();

} // namespace ork
