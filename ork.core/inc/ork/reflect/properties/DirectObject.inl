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

namespace ork::reflect {

template <typename MemberType> //
inline DirectObject<MemberType>::DirectObject(sharedptrtype_t Object::*property)
    : mProperty(property) {
}

template <typename MemberType> //
inline void DirectObject<MemberType>::serialize(serdes::node_ptr_t propnode) const {
  auto serializer     = propnode->_serializer;
  auto parinstance    = propnode->_ser_instance;
  auto child_instance = (parinstance.get()->*mProperty);
  if (child_instance) {
    auto childnode           = serializer->pushNode(_name, serdes::NodeType::OBJECT);
    childnode->_ser_instance = child_instance;
    childnode->_parent       = propnode;
    serializer->serializeObject(childnode);
    serializer->popNode();
  } else {
    propnode->_value.template Set<void*>(nullptr);
    serializer->serializeLeaf(propnode);
  }
}

template <typename MemberType> //
inline void DirectObject<MemberType>::deserialize(serdes::node_ptr_t dsernode) const {
  auto instance     = dsernode->_deser_instance;
  auto deserializer = dsernode->_deserializer;
  auto childnode    = deserializer->deserializeObject(dsernode);
  if (childnode) {
    auto subinstance             = childnode->_deser_instance;
    (instance.get()->*mProperty) = std::dynamic_pointer_cast<rawptrtype_t>(subinstance);
  } else {
    (instance.get()->*mProperty) = nullptr;
  }
}

template <typename MemberType>                            //
inline typename DirectObject<MemberType>::sharedptrtype_t //
DirectObject<MemberType>::access(object_ptr_t instance) const {
  return (instance.get()->*mProperty);
}

template <typename MemberType>                                 //
inline typename DirectObject<MemberType>::sharedconstptrtype_t //
DirectObject<MemberType>::access(object_constptr_t instance) const {
  return (const_cast<Object*>(instance.get())->*mProperty);
}

template <typename MemberType> //
inline void DirectObject<MemberType>::get(
    sharedptrtype_t& value, //
    object_constptr_t instance) const {
  value = (instance.get()->*mProperty);
}
template <typename MemberType> //
inline void DirectObject<MemberType>::set(
    const sharedptrtype_t& value, //
    object_ptr_t instance) const {
  (instance.get()->*mProperty) = value;
}

} // namespace ork::reflect
