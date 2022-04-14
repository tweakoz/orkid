////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/orklut.hpp>

#include <ork/math/TransformNode.h>
#include <ork/ecs/component.h>
#include <ork/ecs/componenttable.h>

#include <ork/kernel/string/ArrayString.h>
#include "entity.h"
#include "archetype.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////
/*
template <typename T> std::shared_ptr<T> ArchComposer::Register() {
  ork::object::ObjectClass* pclass = T::GetClassStatic();
  auto pobj                        = mpArchetype->typedComponent<T>();
  if (0 == pobj) {
    auto X = pclass->createShared();
    pobj   = std::dynamic_pointer_cast<T>(X);
  }
  _components.AddSorted(pclass, pobj);
  std::shared_ptr<T> rval = std::dynamic_pointer_cast<T>(pobj);
  if (rval) {
    rval->RegisterWithScene(mSceneComposer);
  }
  return rval;
}*/

template <typename T> std::shared_ptr<T> Archetype::addComponent() {
  ComponentDataTable::LutType& lut = mComponentDatas;
  ork::object::ObjectClass* pclass = T::GetClassStatic();
  //printf("pclass<%p>\n", (void*) pclass);
  auto pobj = this->typedComponent<T>();
  //printf("pobj<%p>\n", (void*) pobj.get());
  if (0 == pobj) {
    //printf("creating shared\n");
    auto X = pclass->createShared();
    //printf("X<%p>\n", (void*) X.get());
    pobj = std::dynamic_pointer_cast<T>(X);
  }
  lut.AddSorted(pclass, pobj);
  return pobj;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> Archetype::typedComponent() {
  std::shared_ptr<T> rval;
  ComponentDataTable::LutType& lut = mComponentDatas;
  for (auto it : lut) {
    componentdata_constptr_t cd = it.second;
    if (cd && cd->GetClass() == T::GetClassStatic()) {
      auto non_const = std::const_pointer_cast<ComponentData>(cd);
      rval           = std::dynamic_pointer_cast<T>(non_const);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<const T> Archetype::typedComponent() const {
  std::shared_ptr<const T> rval;
  const ComponentDataTable::LutType& lut = mComponentDatas;
  for (auto it : lut) {
    auto cd = it.second;
    if (cd && cd->GetClass() == T::GetClassStatic()) {
      rval = std::dynamic_pointer_cast<const T>(cd);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
