////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/AccessorObject.h>
#include <ork/reflect/properties/AccessorTyped.h>
#include <ork/reflect/properties/AccessorVariant.h>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/DirectObject.h>

#include <ork/reflect/properties/AccessorObjectArray.h>
#include <ork/reflect/properties/AccessorTypedArray.h>
#include <ork/reflect/properties/AccessorVariantArray.h>
#include <ork/reflect/properties/DirectTypedArray.h>
#include <ork/reflect/properties/DirectTypedVector.h>

#include <ork/reflect/properties/AccessorObjectMap.h>
#include <ork/reflect/properties/AccessorTypedMap.h>
#include <ork/reflect/properties/AccessorVariantMap.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/Functor.h>

#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////

namespace component {
class Signal;
}

///////////////////////////////////////////////////////////////////////////

namespace reflect {

///////////////////////////////////////////////////////////////////////////

template <typename ClassType, typename MemberType>
static inline DirectTyped<MemberType>& RegisterPropertyO(
    const char* name,
    MemberType ClassType::*member,
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectTyped<MemberType>(static_cast<MemberType Object::*>(member));

  description.addProperty(name, prop);

  return *prop;
}

///////////////////////////////////////////////////////////////////////////

template <typename ClassType, typename MemberType>
static inline AccessorTyped<MemberType>& RegisterProperty(
    const char* name,
    void (ClassType::*getter)(MemberType&) const,
    void (ClassType::*setter)(const MemberType&),
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorTyped<MemberType>(
      static_cast<void (Object::*)(MemberType&) const>(getter), static_cast<void (Object::*)(const MemberType&)>(setter));

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorObject& RegisterProperty(
    const char* name,
    object_ptr_t (ClassType::*accessor)(),
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorObject(static_cast<object_ptr_t (Object::*)()>(accessor));

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorVariant& RegisterProperty(
    const char* name,
    bool (ClassType::*serialize)(serdes::ISerializer&) const,
    bool (ClassType::*deserialize)(serdes::IDeserializer&),
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorVariant(
      static_cast<bool (Object::*)(serdes::ISerializer&) const>(serialize), //
      static_cast<bool (Object::*)(serdes::IDeserializer&)>(deserialize));

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MemberType, size_t size>
static DirectTypedArray<MemberType>& RegisterArrayProperty(
    const char* name,
    MemberType (ClassType::*pmember)[size],
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectTypedArray<MemberType>(
      // reinterpret_cast is necessary here, the static_cast goes as far as
      // possible, the reinterpret_cast just removes the size info from the
      // array
      reinterpret_cast<MemberType(Object::*)[]>(static_cast<MemberType(Object::*)[size]>(pmember)),
      size);

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MemberType>
static DirectTypedVector<MemberType>& RegisterArrayProperty(
    const char* name,
    MemberType ClassType::*pmember,
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectTypedVector<MemberType>(static_cast<MemberType Object::*>(pmember));

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MemberType>
static inline AccessorTypedArray<MemberType>& RegisterArrayProperty(
    const char* name,
    void (ClassType::*getter)(MemberType&, size_t) const,
    void (ClassType::*setter)(const MemberType&, size_t),
    size_t (ClassType::*counter)() const,
    void (ClassType::*resizer)(size_t newsize) = nullptr,
    Description& description                   = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorTypedArray<MemberType>(
      static_cast<void (Object::*)(MemberType&, size_t) const>(getter),
      static_cast<void (Object::*)(const MemberType&, size_t)>(setter),
      static_cast<size_t (Object::*)() const>(counter),
      static_cast<void (Object::*)(size_t)>(resizer));

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorObjectArray& RegisterArrayProperty(
    const char* name,
    object_ptr_t (ClassType::*accessor)(size_t),
    size_t (ClassType::*counter)() const,
    void (ClassType::*resizer)(size_t newsize) = nullptr,
    Description& description                   = ClassType::GetClassStatic()->Description()) {
  auto prop = new AccessorObjectArray(
      static_cast<object_ptr_t (Object::*)(size_t)>(accessor),
      static_cast<size_t (Object::*)() const>(counter),
      static_cast<void (Object::*)(size_t)>(resizer));

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType>
static inline AccessorVariantArray& RegisterArrayProperty(
    const char* name,
    void (ClassType::*serialize_item)(ISerializer&, size_t) const,
    void (ClassType::*deserialize_item)(serdes::node_ptr_t),
    size_t (ClassType::*count)() const,
    void (ClassType::*resize)(size_t) = nullptr,
    Description& description          = ClassType::GetClassStatic()->Description()) {
  AccessorVariantArray* prop = new AccessorVariantArray(
      static_cast<void (Object::*)(ISerializer&, size_t) const>(serialize_item),
      static_cast<void (Object::*)(serdes::node_ptr_t)>(deserialize_item),
      static_cast<size_t (Object::*)() const>(count),
      static_cast<void (Object::*)(size_t)>(resize));

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename MapType>
static inline DirectTypedMap<MapType>& RegisterMapProperty(
    const char* name,
    MapType ClassType::*member,
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto prop = new DirectTypedMap<MapType>(static_cast<MapType Object::*>(member));
  description.addProperty(name, prop);
  return *prop;
}

template <typename ClassType, typename KeyType, typename ValueType>
static inline AccessorTypedMap<KeyType, ValueType>& RegisterMapProperty(
    const char* name,
    bool (ClassType::*getter)(const KeyType&, int, ValueType&) const,
    void (ClassType::*setter)(const KeyType&, int, const ValueType&),
    void (ClassType::*eraser)(const KeyType&, int),
    void (ClassType::*serializer)(typename AccessorTypedMap<KeyType, ValueType>::SerializationFunction, BidirectionalSerializer&)
        const,
    Description& description = ClassType::GetClassStatic()->Description()) {
  auto _g   = static_cast<bool (Object::*)(const KeyType&, int, ValueType&) const>(getter);
  auto _s   = static_cast<void (Object::*)(const KeyType&, int, const ValueType&)>(setter);
  auto _e   = static_cast<void (Object::*)(const KeyType&, int)>(eraser);
  auto _z   = static_cast<void (Object::*)(
      typename AccessorTypedMap<KeyType, ValueType>::SerializationFunction, BidirectionalSerializer&) const>(serializer);
  auto prop = new AccessorTypedMap<KeyType, ValueType>(_g, _s, _e, _z);

  description.addProperty(name, prop);

  return *prop;
}

template <typename ClassType, typename KeyType>
static inline AccessorObjectMap<KeyType>& RegisterMapProperty(
    const char* name,
    const Object* (ClassType::*get)(const KeyType&, int) const,
    Object* (ClassType::*access)(const KeyType&, int),
    void (ClassType::*eraser)(const KeyType&, int),
    void (ClassType::*serializer)(typename AccessorObjectMap<KeyType>::SerializationFunction, BidirectionalSerializer&) const,
    Description& description = ClassType::GetClassStatic()->Description()) {
  AccessorObjectMap<KeyType>* prop = new AccessorObjectMap<KeyType>(
      static_cast<const Object* (Object::*)(const KeyType&, int) const>(get),
      static_cast<Object* (Object::*)(const KeyType&, int)>(access),
      static_cast<void (Object::*)(const KeyType&, int)>(eraser),
      static_cast<void (Object::*)(typename AccessorObjectMap<KeyType>::SerializationFunction, BidirectionalSerializer&) const>(
          serializer));

  description.addProperty(name, prop);

  return *prop;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename FunctionType> static inline void RegisterFunctor(const char* name, FunctionType function) {
  Description& description = Function<FunctionType>::ClassType::GetClassStatic()->Description();
  description.addFunctor(name, CreateObjectFunctor(function));
}

template <typename ClassType> static void RegisterSignal(const char* name, object::Signal ClassType::*pmember) {
  Description& description = ClassType::GetClassStatic()->Description();
  description.addSignal(name, static_cast<object::Signal Object::*>(pmember));
}

template <typename ClassType, typename FunctionType>
static void RegisterSlot(const char* name, object::AutoSlot ClassType::*pmember, FunctionType function) {
  Description& description = ClassType::GetClassStatic()->Description();
  RegisterFunctor(name, function);
  description.addAutoSlot(name, static_cast<object::AutoSlot Object::*>(pmember));
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
