////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/ObjectCategory.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/rtti/downcast.h>

////////////////////////////////////////////////////////////////
namespace ork { namespace object {
////////////////////////////////////////////////////////////////

bool ObjectCategory::serializeObject(
    reflect::ISerializer& serializer, //
    const rtti::ICastable* value) const {
  const Object* object = rtti::downcast<const Object*>(value);

  return Object::xxxSerialize(object, serializer);
}
////////////////////////////////////////////////////////////////
bool ObjectCategory::serializeObject(
    reflect::ISerializer& serializer, //
    rtti::castable_constptr_t value) const {
  auto object = rtti::downcast<const Object*>(value.get());
  return Object::xxxSerialize(object, serializer);
}
////////////////////////////////////////////////////////////////
bool ObjectCategory::deserializeObject(
    reflect::IDeserializer& deserializer, //
    rtti::castable_ptr_t& outvalue) const {
  reflect::Command command;
  bool cmdok = deserializer.beginCommand(command);
  OrkAssert(cmdok);
  OrkAssert(command.Type() == reflect::Command::EOBJECT);
  // printf("classname<%s>\n", command.Name().c_str());
  Class* the_class = rtti::Class::FindClass(command.Name());
  OrkAssert(the_class);
  ObjectClass* clazz = rtti::downcast<ObjectClass*>(the_class);
  OrkAssert(clazz);
  auto sharedobject = clazz->createShared();
  outvalue          = sharedobject;
  if (false == Object::xxxDeserializeShared(sharedobject, deserializer)) {
    deserializer.endCommand(command);
    return false;
  }
  cmdok = deserializer.endCommand(command);
  OrkAssert(cmdok);
  return true;
}
////////////////////////////////////////////////////////////////
bool ObjectCategory::deserializeObject(
    reflect::IDeserializer& deserializer, //
    rtti::castable_rawptr_t& value) const {
  reflect::Command command;
  bool cmdok = deserializer.beginCommand(command);
  OrkAssert(cmdok);
  OrkAssert(command.Type() == reflect::Command::EOBJECT);
  printf("classname<%s>\n", command.Name().c_str());
  Class* the_class = rtti::Class::FindClass(command.Name());
  OrkAssert(the_class);
  ObjectClass* clazz = rtti::downcast<ObjectClass*>(the_class);
  OrkAssert(clazz);
  value       = clazz->CreateObject();
  auto object = dynamic_cast<Object*>(value);
  if (false == Object::xxxDeserialize(object, deserializer)) {
    deserializer.endCommand(command);
    return false;
  }
  value = object;
  cmdok = deserializer.endCommand(command);
  OrkAssert(cmdok);
  return true;
}

////////////////////////////////////////////////////////////////
ObjectCategory::ObjectCategory(const rtti::RTTIData& data)
    : rtti::Category(data) {
}
////////////////////////////////////////////////////////////////

}} // namespace ork::object
