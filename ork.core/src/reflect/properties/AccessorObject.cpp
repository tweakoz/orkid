////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
void AccessorObject::serialize(serdes::node_ptr_t propnode) const {
  auto serializer     = propnode->_serializer;
  auto parinstance    = propnode->_ser_instance;
  auto nonconst       = std::const_pointer_cast<Object>(parinstance);
  auto child_instance = (nonconst.get()->*_accessor)();
  OrkAssert(false);
  if (child_instance) {
    auto childnode           = serializer->pushNode(_name, serdes::NodeType::OBJECT);
    childnode->_ser_instance = child_instance;
    childnode->_parent       = propnode;
    serializer->serializeObject(childnode);
    serializer->popNode();
  } else {
    propnode->_value.template set<void*>(nullptr);
    serializer->serializeLeaf(propnode);
  }
}
////////////////////////////////////////////////////////////////
void AccessorObject::deserialize(serdes::node_ptr_t dsernode) const {
  auto instance  = dsernode->_deser_instance;
  auto subobject = (instance.get()->*_accessor)();
  // Command command;
  // serializer.beginCommand(command);

  // OrkAssertI(command.Type() == Command::EOBJECT, "AccessorObject::Deserdes::Expected an Object command!\n");

  // if (command.Type() == Command::EOBJECT) {
  // Object::xxxDeserializeShared(dsernode);
  //}

  // serializer.endCommand(command);
}
////////////////////////////////////////////////////////////////
object_ptr_t AccessorObject::access(object_ptr_t instance) const {
  auto subobject = (instance.get()->*_accessor)();
  return subobject;
}
////////////////////////////////////////////////////////////////
object_constptr_t AccessorObject::access(object_constptr_t instance) const {
  auto nonconst  = const_cast<Object*>(instance.get());
  auto subobject = (nonconst->*_accessor)();
  return subobject;
}

}} // namespace ork::reflect
