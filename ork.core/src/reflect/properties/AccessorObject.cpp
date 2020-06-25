////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/reflect/properties/AccessorObject.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork { namespace reflect {

AccessorObject::AccessorObject(Object* (Object::*property)())
    : mObjectAccessor(property) {
}

bool AccessorObject::Serialize(ISerializer& serializer, const Object* object) const {
  const Object* object_property = (const_cast<Object*>(object)->*mObjectAccessor)();
  Object::xxxSerialize(object_property, serializer);
  return true;
}

bool AccessorObject::Deserialize(IDeserializer& serializer, Object* object) const {
  Object* object_property = (object->*mObjectAccessor)();
  Command command;
  serializer.BeginCommand(command);

  OrkAssertI(command.Type() == Command::EOBJECT, "AccessorObject::Deserialize::Expected an Object command!\n");

  if (command.Type() == Command::EOBJECT) {
    Object::xxxDeserialize(object_property, serializer);
  }

  serializer.EndCommand(command);

  return true;
}

Object* AccessorObject::Access(Object* object) const {
  return (object->*mObjectAccessor)();
}

const Object* AccessorObject::Access(const Object* object) const {
  return (const_cast<Object*>(object)->*mObjectAccessor)();
}

}} // namespace ork::reflect
