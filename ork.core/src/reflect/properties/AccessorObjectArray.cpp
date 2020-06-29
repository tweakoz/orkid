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
    object_ptr_t (Object::*accessor)(size_t),
    size_t (Object::*counter)() const,
    void (Object::*resizer)(size_t))
    : _accessor(accessor)
    , _counter(counter)
    , _resizer(resizer) {
}
////////////////////////////////////////////////////////////////
object_ptr_t AccessorObjectArray::accessObject(
    object_ptr_t object, //
    size_t index) const {
  return (object.get()->*_accessor)(index);
}
////////////////////////////////////////////////////////////////
object_constptr_t AccessorObjectArray::accessObject(
    object_constptr_t object, //
    size_t index) const {
  return (const_cast<Object*>(object.get())->*_accessor)(index);
}
////////////////////////////////////////////////////////////////
size_t AccessorObjectArray::count(object_constptr_t object) const {
  return (object.get()->*_counter)();
}
////////////////////////////////////////////////////////////////
void AccessorObjectArray::deserializeElement(IDeserializer::node_ptr_t desernode) const {
  // Command object_command;

  // deserializer.beginCommand(object_command);

  // auto child_object = accessObject(parent_object, dsernode._index);

  // if (object_command.Type() != Command::EOBJECT or //
  //  nullptr == child_object or                   //
  // object_command.Name() != child_object->GetClass()->Name()) {
  // OrkAssert(false);
  //  }

  // Object::xxxDeserializeShared(child_object, deserializer);
  // deserializer.endCommand(object_command);
}
////////////////////////////////////////////////////////////////
void AccessorObjectArray::serializeElement(ISerializer::node_ptr_t node) const {
  // auto child_object = accessObject(object, index);
  // return Object::xxxSerializeShared(child_object, serializer);
}
////////////////////////////////////////////////////////////////
void AccessorObjectArray::resize(
    object_ptr_t obj, //
    size_t size) const {
  OrkAssert(_resizer != nullptr);
  (obj.get()->*_resizer)(size);
}
////////////////////////////////////////////////////////////////
void AccessorObjectArray::deserialize(ork::reflect::IDeserializer::node_ptr_t) const {
  OrkAssert(false);
}
////////////////////////////////////////////////////////////////
void AccessorObjectArray::serialize(ISerializer::node_ptr_t node) const {
  OrkAssert(false);
}
////////////////////////////////////////////////////////////////
}} // namespace ork::reflect
