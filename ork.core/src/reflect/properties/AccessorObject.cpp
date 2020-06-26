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
////////////////////////////////////////////////////////////////
AccessorObject::AccessorObject(object_ptr_t (Object::*property)())
    : _accessor(property) {
}
////////////////////////////////////////////////////////////////
void AccessorObject::serialize(
    ISerializer& serializer, //
    object_constptr_t owner) const {
  auto nonconst  = std::const_pointer_cast<Object>(owner);
  auto subobject = (nonconst.get()->*_accessor)();
  Object::xxxSerializeShared(subobject, serializer);
}
////////////////////////////////////////////////////////////////
void AccessorObject::deserialize(
    IDeserializer& serializer, //
    object_ptr_t owner) const {
  auto subobject = (owner.get()->*_accessor)();
  Command command;
  serializer.beginCommand(command);

  OrkAssertI(command.Type() == Command::EOBJECT, "AccessorObject::Deserialize::Expected an Object command!\n");

  if (command.Type() == Command::EOBJECT) {
    Object::xxxDeserializeShared(subobject, serializer);
  }

  serializer.endCommand(command);
}
////////////////////////////////////////////////////////////////
object_ptr_t AccessorObject::Access(Object* owner) const {
  auto subobject = (owner->*_accessor)();
  return subobject;
}
////////////////////////////////////////////////////////////////
object_constptr_t AccessorObject::Access(const Object* owner) const {
  auto nonconst  = const_cast<Object*>(owner);
  auto subobject = (nonconst->*_accessor)();
  return subobject;
}

}} // namespace ork::reflect
