////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "scene.h"
#include "archetype.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////
/// Simulation is all the work data associated with running a scene
/// this might be subclassed
///////////////////////////////////////////////////////////////////////////////

template <typename T> T* Simulation::findTypedEntityComponent(const PoolString& entname) const {
  T* pret      = 0;
  Entity* pent = findEntity(entname);
  if (pent) {
    pret = pent->typedComponent<T>();
  }
  printf("fINDENT<%s:%p> comp<%p>\n", entname.c_str(), pent, pret);
  return pret;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T* Simulation::findSystem() const {
  T* rval            = nullptr;
  systemkey_t pclass = T::SystemType;
  _systems.atomicOp([&](const SystemLut& syslut) {
    auto it    = syslut.find(pclass);
    bool found = (it != syslut.end());
    if (found)
      rval = dynamic_cast<T*>(it->second);
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T* Simulation::findTypedEntityComponent(const char* entname) const {
  return findTypedEntityComponent<T>(AddPooledString(entname));
}


} // namespace ork::ecs {
