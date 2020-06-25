////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/AccessorObjectArray.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/ISerializer.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

////////////////////////////////////////////////////////////////
AccessorObjectArray::AccessorObjectArray(
    Object* (Object::*accessor)(size_t),
    size_t (Object::*counter)() const,
    void (Object::*resizer)(size_t))
    : mAccessor(accessor)
    , mCounter(counter)
    , mResizer(resizer) {
}
////////////////////////////////////////////////////////////////
Object* AccessorObjectArray::AccessObject(
    Object* object, //
    size_t index) const {
  return (object->*mAccessor)(index);
}
////////////////////////////////////////////////////////////////
const Object* AccessorObjectArray::AccessObject(
    const Object* object, //
    size_t index) const {
  return (const_cast<Object*>(object)->*mAccessor)(index);
}
////////////////////////////////////////////////////////////////
size_t AccessorObjectArray::Count(const Object* object) const {
  return (object->*mCounter)();
}
////////////////////////////////////////////////////////////////
bool AccessorObjectArray::DeserializeItem(
    IDeserializer& deserializer, //
    Object* parent_object,
    size_t index) const {
  Command object_command;

  if (false == deserializer.BeginCommand(object_command))
    return false;

  Object* child_object = AccessObject(parent_object, index);

  if (object_command.Type() != Command::EOBJECT or //
      nullptr == child_object or                   //
      object_command.Name() != child_object->GetClass()->Name()) {
    deserializer.EndCommand(object_command);
    return false;
  }

  if (false == Object::xxxDeserialize(child_object, deserializer))
    return false;

  if (false == deserializer.EndCommand(object_command))
    return false;

  return true;
}
////////////////////////////////////////////////////////////////
bool AccessorObjectArray::SerializeItem(
    ISerializer& serializer, //
    const Object* object,
    size_t index) const {
  auto child_object = AccessObject(object, index);
  return Object::xxxSerialize(child_object, serializer);
}
////////////////////////////////////////////////////////////////
bool AccessorObjectArray::Resize(
    Object* obj, //
    size_t size) const {
  if (mResizer != 0) {
    (obj->*mResizer)(size);
    return true;
  } else {
    return size == Count(obj);
  }
}
////////////////////////////////////////////////////////////////
bool AccessorObjectArray::Deserialize(ork::reflect::IDeserializer&, ork::Object*) const {
  OrkAssert(false);
  return false;
}
////////////////////////////////////////////////////////////////
bool AccessorObjectArray::Serialize(ork::reflect::ISerializer&, ork::Object const*) const {
  OrkAssert(false);
  return false;
}
////////////////////////////////////////////////////////////////
}} // namespace ork::reflect
