////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>

#include <ork/math/TransformNode.h>
#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>

#include <ork/kernel/string/ArrayString.h>
#include "entity.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ent {

///////////////////////////////////////////////////////////////////////////////

template <typename T> const T* EntData::GetTypedComponent() const { return mArchetype ? mArchetype->GetTypedComponent<T>() : 0; }

///////////////////////////////////////////////////////////////////////////////
// an INSTANCE of an EntData is an Entity
///////////////////////////////////////////////////////////////////////////////

template <typename T> T* Entity::GetTypedComponent(bool bsubclass) {
  T* rval = 0;
  ComponentTable::LutType& lut = mComponentTable.GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    ComponentInst* cd = (*it).second;
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

template <typename T> const T* Entity::GetTypedComponent(bool bsubclass) const {
  const T* rval = 0;
  const ComponentTable::LutType& lut = mComponentTable.GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    ComponentInst* cd = (*it).second;
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

template <typename T> T* ArchComposer::Register() {
  ork::object::ObjectClass* pclass = T::GetClassStatic();
  ork::Object* pobj = mpArchetype->GetTypedComponent<T>();
  if (0 == pobj) {
    pobj = rtti::autocast(pclass->CreateObject());
  }
  _components.AddSorted(pclass, pobj);
  T* rval = rtti::autocast(pobj);
  if (rval) {
    rval->RegisterWithScene(mSceneComposer);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T* Archetype::GetTypedComponent() {
  T* rval = 0;
  ComponentDataTable::LutType& lut = mComponentDataTable.GetComponents();
  for (ComponentDataTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    ComponentData* cd = (*it).second;
    if (cd && cd->GetClass() == T::GetClassStatic()) {
      rval = rtti::safe_downcast<T*>(cd);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const T* Archetype::GetTypedComponent() const {
  const T* rval = 0;
  const ComponentDataTable::LutType& lut = mComponentDataTable.GetComponents();
  for (ComponentDataTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    ComponentData* cd = (*it).second;
    if (cd && cd->GetClass() == T::GetClassStatic()) {
      rval = rtti::safe_downcast<const T*>(cd);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ent
