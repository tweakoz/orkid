////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/reflect/properties/AccessorSharedObject.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork { namespace reflect {

AccessorSharedObject::AccessorSharedObject(object_ptr_t (Object::*property)())
    : mObjectAccessor(property) {
}

bool AccessorSharedObject::Serialize(ISerializer& serializer, const Object* object) const {
  auto obj_non_const                = const_cast<Object*>(object);
  object_constptr_t object_property = (obj_non_const->*mObjectAccessor)();
  Object::xxxSerializeShared(object_property, serializer);
  return true;
}

bool AccessorSharedObject::Deserialize(IDeserializer& serializer, Object* object) const {
  object_ptr_t object_property = (object->*mObjectAccessor)();
  Command command;
  serializer.BeginCommand(command);

  OrkAssertI(command.Type() == Command::EOBJECT, "AccessorSharedObject::Deserialize::Expected an Object command!\n");

  if (command.Type() == Command::EOBJECT) {
    Object::xxxDeserializeShared(object_property, serializer);
  }

  serializer.EndCommand(command);

  return true;
}

object_ptr_t AccessorSharedObject::Access(Object* object) const {
  return (object->*mObjectAccessor)();
}

object_constptr_t AccessorSharedObject::Access(const Object* object) const {
  return (const_cast<Object*>(object)->*mObjectAccessor)();
}

}} // namespace ork::reflect
