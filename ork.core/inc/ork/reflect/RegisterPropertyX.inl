////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#include "RegisterProperty.h"
namespace ork::object {
///////////////////////////////////////////////////////////////////////////
template <typename T> inline PropertyModifier* PropertyModifier::annotate(const ConstString& key, T value) {
  reflect::ObjectProperty::anno_t anno;
  anno.Set<T>(value);
  _property->annotate(key, anno);
  return this;
}
///////////////////////////////////////////////////////////////////////////
inline PropertyModifier* PropertyModifier::Annotate(const ConstString& key, ConstString value) {
  _property->annotate(key, value);
  return this;
}
///////////////////////////////////////////////////////////////////////////
PropertyModifier* PropertyModifier::operator->() {
  return this;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberType>
inline object::PropertyModifier object::ObjectClass::memberProperty(const char* name, MemberType ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<MemberType Object::*>(member);
  modder._property  = new reflect::DirectTyped<MemberType>(typed_member);
  _description.AddProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType>
inline object::PropertyModifier object::ObjectClass::sharedObjectProperty( //
    const char* name,                                                      //
    object_ptr_t ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<object_ptr_t Object::*>(member);
  modder._property  = new reflect::DirectSharedObject(typed_member);
  _description.AddProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberType>
inline object::PropertyModifier object::ObjectClass::accessorProperty(
    const char* name,
    void (ClassType::*getter)(MemberType&) const,
    void (ClassType::*setter)(const MemberType&)) {
  object::PropertyModifier modder;
  auto typed_getter = static_cast<void (Object::*)(MemberType&) const>(getter);
  auto typed_setter = static_cast<void (Object::*)(const MemberType&)>(setter);
  modder._property  = new reflect::AccessorTyped<MemberType>(typed_getter, typed_setter);
  _description.AddProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType>
inline PropertyModifier object::ObjectClass::floatProperty(const char* name, float_range rng, float ClassType::*member) {
  auto rval = memberProperty<ClassType, float>(name, member);
  rval->annotate("editor.range", rng);
  return rval;
}
///////////////////////////////////////////////////////////////////////////
} // namespace ork::object
