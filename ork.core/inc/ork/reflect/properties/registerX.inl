////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#include "register.h"
#include "LambdaTyped.inl"
#include "DirectObject.inl"
#include "DirectObjectMap.inl"
#include "DirectTypedArray.hpp"
#include "DirectTypedVector.hpp"
#include "AccessorTyped.hpp"

namespace ork::object {
///////////////////////////////////////////////////////////////////////////
template <typename T> inline PropertyModifier* PropertyModifier::annotate(const ConstString& key, T value) {
  reflect::ObjectProperty::anno_t anno;
  anno.set<T>(value);
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
inline object::PropertyModifier object::ObjectClass::directProperty(const char* name, MemberType ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<MemberType Object::*>(member);
  modder._property  = new reflect::DirectTyped<MemberType>(typed_member);
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberMapType>
inline object::PropertyModifier object::ObjectClass::directMapProperty(
    const char* name, //
    MemberMapType ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<MemberMapType Object::*>(member);
  modder._property  = new reflect::DirectTypedMap<MemberMapType>(typed_member);
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberArrayType>
inline object::PropertyModifier object::ObjectClass::directArrayProperty(
    const char* name, //
    MemberArrayType ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<MemberArrayType Object::*>(member);
  modder._property  = new reflect::DirectTypedArray<MemberArrayType>(typed_member);
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberVectorType>
inline object::PropertyModifier object::ObjectClass::directVectorProperty(
    const char* name, //
    MemberVectorType ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<MemberVectorType Object::*>(member);
  modder._property  = new reflect::DirectTypedVector<MemberVectorType>(typed_member);
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberType>
inline object::PropertyModifier object::ObjectClass::directObjectMapProperty(const char* name, MemberType ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<MemberType Object::*>(member);
  modder._property  = new reflect::DirectObjectMap(typed_member);
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberType>
inline object::PropertyModifier object::ObjectClass::directObjectProperty( //
    const char* name,                                                      //
    MemberType ClassType::*member) {
  object::PropertyModifier modder;
  auto typed_member = static_cast<MemberType Object::*>(member);
  modder._property  = new reflect::DirectObject<MemberType>(typed_member);
  _description.addProperty(name, modder._property);
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
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType, typename MemberType>
inline object::PropertyModifier object::ObjectClass::lambdaProperty(
    const char* name,
    std::function<void(const ClassType*, MemberType&)> getter,
    std::function<void(ClassType*, const MemberType&)> setter) {
  object::PropertyModifier modder;
  modder._property = new reflect::LambdaTyped<ClassType, MemberType>(getter, setter);
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType>
inline object::PropertyModifier object::ObjectClass::accessorVariant(
    const char* name,
    bool (ClassType::*serialize)(reflect::serdes::ISerializer&) const,
    bool (ClassType::*deserialize)(reflect::serdes::IDeserializer&)) {
  object::PropertyModifier modder;
  auto typed_getter = static_cast<bool (Object::*)(reflect::serdes::ISerializer&) const>(serialize);
  auto typed_setter = static_cast<bool (Object::*)(reflect::serdes::IDeserializer&)>(deserialize);
  modder._property  = new reflect::AccessorVariant(typed_getter, typed_setter);
  _description.addProperty(name, modder._property);
  return modder;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType>
inline PropertyModifier object::ObjectClass::floatProperty(const char* name, float_range rng, float ClassType::*member) {
  auto rval = directProperty<ClassType, float>(name, member);
  rval->annotate("editor.range", rng);
  return rval;
}
///////////////////////////////////////////////////////////////////////////
template <typename ClassType>
inline PropertyModifier object::ObjectClass::intProperty(const char* name, int_range rng, int ClassType::*member) {
  auto rval = directProperty<ClassType, int>(name, member);
  rval->annotate("editor.range", rng);
  return rval;
}
///////////////////////////////////////////////////////////////////////////
} // namespace ork::object
