////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/AccessorPropertyObject.h>
#include <ork/reflect/properties/AccessorPropertySharedObject.h>
#include <ork/reflect/properties/AccessorPropertyType.h>
#include <ork/reflect/properties/AccessorPropertyVariant.h>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/DirectSharedObject.h>

#include <ork/reflect/properties/AccessorArrayPropertyObject.h>
#include <ork/reflect/properties/AccessorArrayPropertyType.h>
#include <ork/reflect/properties/AccessorArrayPropertyVariant.h>
#include <ork/reflect/properties/DirectArrayTyped.h>
#include <ork/reflect/properties/DirectVectorTyped.h>

#include <ork/reflect/properties/AccessorMapPropertyObject.h>
#include <ork/reflect/properties/AccessorMapPropertyType.h>
#include <ork/reflect/properties/AccessorMapPropertyVariant.h>
#include <ork/reflect/properties/DirectMapTyped.h>
#include <ork/reflect/Functor.h>

#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>

namespace ork {

class Object;

///////////////////////////////////////////////////////////////////////////

namespace component {
class Signal;
}

///////////////////////////////////////////////////////////////////////////

namespace reflect {

///////////////////////////////////////////////////////////////////////////

template <typename ClassType, typename MemberType>
static inline DirectPropertyType<MemberType>& RegisterProperty(
    const char* name,
    MemberType ClassType::*member,
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectPropertyType<MemberType>(static_cast<MemberType Object::*>(member));

  description.AddProperty(name, prop);

  return *prop;
}

///////////////////////////////////////////////////////////////////////////

template <typename ClassType, typename MemberType>
static inline AccessorPropertyType<MemberType>& RegisterProperty(
    const char* name,
    void (ClassType::*getter)(MemberType&) const,
    void (ClassType::*setter)(const MemberType&),
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorPropertyType<MemberType>(
      static_cast<void (Object::*)(MemberType&) const>(getter), static_cast<void (Object::*)(const MemberType&)>(setter));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorPropertyObject& RegisterProperty(
    const char* name,
    Object* (ClassType::*accessor)(),
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorPropertyObject(static_cast<Object* (Object::*)()>(accessor));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorPropertySharedObject& RegisterProperty(
    const char* name,
    object_ptr_t (ClassType::*accessor)(),
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorPropertySharedObject(static_cast<object_ptr_t (Object::*)()>(accessor));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorPropertyVariant& RegisterProperty(
    const char* name,
    bool (ClassType::*serialize)(ISerializer&) const,
    bool (ClassType::*deserialize)(IDeserializer&),
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorPropertyVariant(
      static_cast<bool (Object::*)(ISerializer&) const>(serialize), static_cast<bool (Object::*)(IDeserializer&)>(deserialize));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MemberType, size_t size>
static DirectArrayPropertyType<MemberType>& RegisterArrayProperty(
    const char* name,
    MemberType (ClassType::*pmember)[size],
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectArrayPropertyType<MemberType>(
      // reinterpret_cast is necessary here, the static_cast goes as far as
      // possible, the reinterpret_cast just removes the size info from the
      // array
      reinterpret_cast<MemberType(Object::*)[]>(static_cast<MemberType(Object::*)[size]>(pmember)),
      size);

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MemberType>
static DirectVectorPropertyType<MemberType>& RegisterArrayProperty(
    const char* name,
    MemberType ClassType::*pmember,
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectVectorPropertyType<MemberType>(static_cast<MemberType Object::*>(pmember));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MemberType>
static inline AccessorArrayPropertyType<MemberType>& RegisterArrayProperty(
    const char* name,
    void (ClassType::*getter)(MemberType&, size_t) const,
    void (ClassType::*setter)(const MemberType&, size_t),
    size_t (ClassType::*counter)() const,
    void (ClassType::*resizer)(size_t newsize) = nullptr,
    Description& description                   = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorArrayPropertyType<MemberType>(
      static_cast<void (Object::*)(MemberType&, size_t) const>(getter),
      static_cast<void (Object::*)(const MemberType&, size_t)>(setter),
      static_cast<size_t (Object::*)() const>(counter),
      static_cast<void (Object::*)(size_t)>(resizer));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorArrayPropertyObject& RegisterArrayProperty(
    const char* name,
    Object* (ClassType::*accessor)(size_t),
    size_t (ClassType::*counter)() const,
    void (ClassType::*resizer)(size_t newsize) = nullptr,
    Description& description                   = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorArrayPropertyObject(
      static_cast<Object* (Object::*)(size_t)>(accessor),
      static_cast<size_t (Object::*)() const>(counter),
      static_cast<void (Object::*)(size_t)>(resizer));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorArrayPropertyVariant& RegisterArrayProperty(
    const char* name,
    bool (ClassType::*serialize_item)(ISerializer&, size_t) const,
    bool (ClassType::*deserialize_item)(IDeserializer&, size_t),
    size_t (ClassType::*count)() const,
    bool (ClassType::*resize)(size_t) = nullptr,
    Description& description          = ClassType::GetClassStatic()->Description()) {
  AccessorArrayPropertyVariant* prop = new AccessorArrayPropertyVariant(
      static_cast<bool (Object::*)(ISerializer&, size_t) const>(serialize_item),
      static_cast<bool (Object::*)(IDeserializer&, size_t)>(deserialize_item),
      static_cast<size_t (Object::*)() const>(count),
      static_cast<bool (Object::*)(size_t)>(resize));

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MapType>
static inline DirectMapPropertyType<MapType>& RegisterMapProperty(
    const char* name,
    MapType ClassType::*member,
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectMapPropertyType<MapType>(static_cast<MapType Object::*>(member));
  description.AddProperty(name, prop);
  return *prop;
}

template <typename ClassType, typename KeyType, typename ValueType>
static inline AccessorMapPropertyType<KeyType, ValueType>& RegisterMapProperty(
    const char* name,
    bool (ClassType::*getter)(const KeyType&, int, ValueType&) const,
    void (ClassType::*setter)(const KeyType&, int, const ValueType&),
    void (ClassType::*eraser)(const KeyType&, int),
    void (ClassType::*serializer)(
        typename AccessorMapPropertyType<KeyType, ValueType>::SerializationFunction,
        BidirectionalSerializer&) const,
    Description& description = ClassType::GetClassStatic()->Description()) {

  auto _g = static_cast<bool (Object::*)(const KeyType&, int, ValueType&) const>(getter);
  auto _s = static_cast<void (Object::*)(const KeyType&, int, const ValueType&)>(setter);
  auto _e = static_cast<void (Object::*)(const KeyType&, int)>(eraser);
  auto _z = static_cast<void (Object::*)(
      typename AccessorMapPropertyType<KeyType, ValueType>::SerializationFunction, BidirectionalSerializer&) const>(
      serializer);
  auto prop = new AccessorMapPropertyType<KeyType, ValueType>(_g, _s, _e, _z);

  description.AddProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename KeyType>
static inline AccessorMapPropertyObject<KeyType>& RegisterMapProperty(
    const char* name,
    const Object* (ClassType::*get)(const KeyType&, int) const,
    Object* (ClassType::*access)(const KeyType&, int),
    void (ClassType::*eraser)(const KeyType&, int),
    void (
        ClassType::*serializer)(typename AccessorMapPropertyObject<KeyType>::SerializationFunction, BidirectionalSerializer&)
        const,
    Description& description = ClassType::GetClassStatic()->Description()) {
  AccessorMapPropertyObject<KeyType>* prop = new AccessorMapPropertyObject<KeyType>(
      static_cast<const Object* (Object::*)(const KeyType&, int) const>(get),
      static_cast<Object* (Object::*)(const KeyType&, int)>(access),
      static_cast<void (Object::*)(const KeyType&, int)>(eraser),
      static_cast<void (Object::*)(
          typename AccessorMapPropertyObject<KeyType>::SerializationFunction, BidirectionalSerializer&) const>(serializer));

  description.AddProperty(name, prop);

  return *prop;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename FunctionType> static inline void RegisterFunctor(const char* name, FunctionType function) {
  Description& description = Function<FunctionType>::ClassType::GetClassStatic()->Description();
  description.AddFunctor(name, CreateObjectFunctor(function));
}

template <typename ClassType> static void RegisterSignal(const char* name, object::Signal ClassType::*pmember) {
  Description& description = ClassType::GetClassStatic()->Description();
  description.AddSignal(name, static_cast<object::Signal Object::*>(pmember));
}

template <typename ClassType, typename FunctionType>
static void RegisterSlot(const char* name, object::AutoSlot ClassType::*pmember, FunctionType function) {
  Description& description = ClassType::GetClassStatic()->Description();
  RegisterFunctor(name, function);
  description.AddAutoSlot(name, static_cast<object::AutoSlot Object::*>(pmember));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename ClassType> inline void annotateProperty(const char* PropName, const char* Key, const char* Val) {
  Description& description = ClassType::GetClassStatic()->Description();
  description.annotateProperty(PropName, Key, Val);
}
template <typename ClassType> inline void annotatePropertyForEditor(const char* PropName, const char* Key, const char* Val) {
  Description& description = ClassType::GetClassStatic()->Description();
  description.annotateProperty(PropName, Key, Val);
}
template <typename ClassType> inline void annotateClassForEditor(const char* Key, const Description::anno_t& Val) {
  Description& description = ClassType::GetClassStatic()->Description();
  description.annotateClass(Key, Val);
}

struct OpMap {
  typedef std::function<void(Object*)> lambda_t;
  std::map<std::string, lambda_t> mLambdaMap;
};

///////////////////////////////////////////////////////////////////////////////
// other prop registration helpers
///////////////////////////////////////////////////////////////////////////////

template <typename T> void RegisterFloatMinMaxProp(float T::*member, const char* prop_name, const char* fmin, const char* fmax) {
  RegisterProperty(prop_name, member);
  annotatePropertyForEditor<T>(prop_name, "editor.range.min", fmin);
  annotatePropertyForEditor<T>(prop_name, "editor.range.max", fmax);
}
template <typename T> void RegisterIntMinMaxProp(int T::*member, const char* prop_name, const char* imin, const char* imax) {
  RegisterProperty(prop_name, member);
  annotatePropertyForEditor<T>(prop_name, "editor.range.min", imin);
  annotatePropertyForEditor<T>(prop_name, "editor.range.max", imax);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace reflect

} // namespace ork
