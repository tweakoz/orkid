////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/math/TransformNode.h>

#include "component.h"
#include "componenttable.h"
#include "entity.h"
#include "archetype.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<const T> SpawnData::typedComponent() const {
  return _archetype ? _archetype->typedComponent<T>() : 0;
}

///////////////////////////////////////////////////////////////////////////////
// an INSTANCE of an EntData is an Entity
///////////////////////////////////////////////////////////////////////////////

template <typename T> T* Entity::typedComponent(bool bsubclass) {
  T* rval                      = 0;
  ComponentTable::LutType& lut = mComponentTable.GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* cd = (*it).second;
    if (bsubclass) {
      if (cd->GetClass()->IsSubclassOf(T::GetClassStatic())) {
        rval = rtti::safe_downcast<T*>(cd);
      }
    } else if (cd->GetClass() == T::GetClassStatic()) {
      rval = rtti::safe_downcast<T*>(cd);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const T* Entity::typedComponent(bool bsubclass) const {
  const T* rval                      = 0;
  const ComponentTable::LutType& lut = mComponentTable.GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* cd = (*it).second;
    if (bsubclass) {
      if (cd->GetClass()->IsSubclassOf(T::GetClassStatic())) {
        rval = rtti::safe_downcast<const T*>(cd);
      }
    } else if (cd->GetClass() == T::GetClassStatic()) {
      rval = rtti::safe_downcast<const T*>(cd);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
