////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/reflect/properties/DirectSharedObject.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork { namespace reflect {

DirectSharedObject::DirectSharedObject(object_ptr_t Object::*property)
    : mProperty(property) {
}

bool DirectSharedObject::Serialize(ISerializer& serializer, const Object* object) const {
  // const Object* object_property = (const_cast<Object*>(object)->*mProperty)();
  // Object::xxxSerialize(object_property, serializer);
  OrkAssert(false);
  return true;
}

bool DirectSharedObject::Deserialize(IDeserializer& serializer, Object* object) const {
  OrkAssert(false);
  /*Object* object_property = (object->*mProperty)();
  Command command;
  serializer.BeginCommand(command);

  OrkAssertI(command.Type() == Command::EOBJECT, "DirectSharedObject::Deserialize::Expected an Object command!\n");

  if (command.Type() == Command::EOBJECT) {
    Object::xxxDeserialize(object_property, serializer);
  }

  serializer.EndCommand(command);*/

  return true;
}

object_ptr_t DirectSharedObject::Access(Object* object) const {
  return (object->*mProperty);
}

object_constptr_t DirectSharedObject::Access(const Object* object) const {
  return (const_cast<Object*>(object)->*mProperty);
}

void DirectSharedObject::get(
    object_ptr_t& value, //
    const Object* object) const {
  value = (object->*mProperty);
}
void DirectSharedObject::set(
    object_ptr_t const& value, //
    Object* object) const {
  (object->*mProperty) = value;
}

}} // namespace ork::reflect
