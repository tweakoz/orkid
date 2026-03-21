////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/ecs/ReferenceArchetype.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>

ImplementReflectionX(ork::ecs::ReferenceArchetype, "EcsReferenceArchetype");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::describeX(SceneObjectClass* clazz) {
  clazz->directProperty("ImportNamespace", &ReferenceArchetype::_importNamespace);
  clazz->directProperty("ArchetypeName", &ReferenceArchetype::_archetypeName);
}

///////////////////////////////////////////////////////////////////////////////

ReferenceArchetype::ReferenceArchetype() {
}

///////////////////////////////////////////////////////////////////////////////

archetype_constptr_t ReferenceArchetype::_resolve(Simulation* sim) const {
  return sim->controller()->findImportedArchetype(_importNamespace, _archetypeName);
}

///////////////////////////////////////////////////////////////////////////////
// All do* overrides delegate to resolved archetype's do* (protected, accessible via inheritance).
// This avoids double-execution of non-virtual wrapper logic.
///////////////////////////////////////////////////////////////////////////////

void ReferenceArchetype::doComposeEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doComposeEntity(sim, ent);
}

void ReferenceArchetype::doDecomposeEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doDecomposeEntity(sim, ent);
}

void ReferenceArchetype::doLinkEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doLinkEntity(sim, ent);
}

void ReferenceArchetype::doUnlinkEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doUnlinkEntity(sim, ent);
}

void ReferenceArchetype::doStageEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doStageEntity(sim, ent);
}

void ReferenceArchetype::doUnstageEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doUnstageEntity(sim, ent);
}

void ReferenceArchetype::doActivateEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doActivateEntity(sim, ent);
}

void ReferenceArchetype::doDeactivateEntity(Simulation* sim, Entity* ent) const {
  auto arch = _resolve(sim);
  OrkAssert(arch);
  arch->doDeactivateEntity(sim, ent);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
