////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "archetype.h"

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////
/// ReferenceArchetype is a local proxy in the primary SceneData
/// that delegates to an Archetype in an imported SceneData.
/// Fully immutable after deserialization — resolves lazily via
/// Simulation → Controller → _importedScenes at call time.
///////////////////////////////////////////////////////////////////////////////

struct ReferenceArchetype final : public Archetype {
  DeclareConcreteX(ReferenceArchetype, Archetype);

public:
  ReferenceArchetype();

  std::string _importNamespace;    // e.g. "env" or "env:props"
  std::string _archetypeName;      // e.g. "TreeArchetype"

protected:
  // Override do* methods — delegate to resolved archetype's do* methods (protected access).
  // This ensures non-virtual wrapper logic (e.g. activateEntity's deactivate-first)
  // runs exactly once, not twice.
  void doComposeEntity(Simulation*, Entity*) const override;
  void doDecomposeEntity(Simulation*, Entity*) const override;
  void doLinkEntity(Simulation*, Entity*) const override;
  void doUnlinkEntity(Simulation*, Entity*) const override;
  void doStageEntity(Simulation*, Entity*) const override;
  void doUnstageEntity(Simulation*, Entity*) const override;
  void doActivateEntity(Simulation*, Entity*) const override;
  void doDeactivateEntity(Simulation*, Entity*) const override;

private:
  archetype_constptr_t _resolve(Simulation* sim) const;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
