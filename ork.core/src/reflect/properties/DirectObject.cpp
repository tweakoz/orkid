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

void DirectObject::serialize(ISerializer::node_ptr_t propnode) const {
  auto serializer     = propnode->_serializer;
  auto parinstance    = propnode->_instance;
  auto child_instance = (parinstance.get()->*mProperty);
  if (child_instance) {
    auto childnode       = serializer->pushObjectNode(_name);
    childnode->_instance = child_instance;
    childnode->_parent   = propnode;
    serializer->serializeObject(childnode);
    serializer->popNode();
  } else {
    propnode->_value.template Set<void*>(nullptr);
    serializer->serializeLeaf(propnode);
  }
}

void DirectObject::deserialize(IDeserializer::node_ptr_t dsernode) const {
  OrkAssert(false);
  /*Object* object_property = (object->*mProperty)();
  Command command;
  serializer.beginCommand(command);

  OrkAssertI(command.Type() == Command::EOBJECT, "DirectObject::Deserdes::Expected an Object command!\n");

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
