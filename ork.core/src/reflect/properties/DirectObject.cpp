////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/reflect/properties/DirectObject.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>

namespace ork { namespace reflect {

DirectObject::DirectObject(object_ptr_t Object::*property)
    : mProperty(property) {
}

void DirectObject::serialize(ISerializer& serializer, object_constptr_t instance) const {
  // const Object* object_property = (const_cast<Object*>(object)->*mProperty)();
  // Object::xxxSerialize(object_property, serializer);
  OrkAssert(false);
}

void DirectObject::deserialize(IDeserializer& serializer, object_ptr_t instance) const {
  OrkAssert(false);
  /*Object* object_property = (object->*mProperty)();
  Command command;
  serializer.beginCommand(command);

  OrkAssertI(command.Type() == Command::EOBJECT, "DirectObject::Deserialize::Expected an Object command!\n");

  if (command.Type() == Command::EOBJECT) {
    Object::xxxDeserialize(object_property, serializer);
  }

  serializer.endCommand(command);*/
}

object_ptr_t DirectObject::access(object_ptr_t instance) const {
  return (instance.get()->*mProperty);
}

object_constptr_t DirectObject::access(object_constptr_t instance) const {
  return (const_cast<Object*>(instance.get())->*mProperty);
}

void DirectObject::get(
    object_ptr_t& value, //
    object_constptr_t instance) const {
  value = (instance.get()->*mProperty);
}
void DirectObject::set(
    object_ptr_t const& value, //
    object_ptr_t instance) const {
  (instance.get()->*mProperty) = value;
}

}} // namespace ork::reflect
