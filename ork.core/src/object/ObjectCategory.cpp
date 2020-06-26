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

void ObjectCategory::serializeObject(
    reflect::ISerializer& serializer, //
    object_constptr_t object) const {
  Object::xxxSerializeShared(object, serializer);
}
////////////////////////////////////////////////////////////////
void ObjectCategory::deserializeObject(
    reflect::IDeserializer& deserializer, //
    object_ptr_t& outvalue) const {
  reflect::Command command;
  deserializer.beginCommand(command);
  OrkAssert(command.Type() == reflect::Command::EOBJECT);
  // printf("classname<%s>\n", command.Name().c_str());
  Class* the_class = rtti::Class::FindClass(command.Name());
  OrkAssert(the_class);
  ObjectClass* clazz = rtti::downcast<ObjectClass*>(the_class);
  OrkAssert(clazz);
  auto sharedobject = clazz->createShared();
  outvalue          = sharedobject;
  Object::xxxDeserializeShared(sharedobject, deserializer);
  deserializer.endCommand(command);
}
////////////////////////////////////////////////////////////////
ObjectCategory::ObjectCategory(const rtti::RTTIData& data)
    : rtti::Category(data) {
}
////////////////////////////////////////////////////////////////

}} // namespace ork::object
