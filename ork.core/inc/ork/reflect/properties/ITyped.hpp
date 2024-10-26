////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
///////////////////////////////////////////////////////////////////////////////
#include <concepts>
#include <ork/orktypes.h>
#include "ITyped.h"
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
//#include <ork/reflect/serialize/ShallowDeserializer.h>
//#include <ork/reflect/serialize/ShallowSerializer.h>
//#include <ork/reflect/serialize/JsonDeserializer.h>
//#include <ork/reflect/serialize/JsonSerializer.h>
#include "codec.inl"
///////////////////////////////////////////////////////////////////////////////
template<typename T>
concept is_not_enum = !std::is_enum_v<T>;
// Concept to check if custom serialization is not enabled for T
template<typename T>
concept no_custom_serdes = !::ork::use_custom_serdes<T>::enable;
///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
///////////////////////////////////////////////////////////////////////////////
template <typename T> //
void ITyped<T>::deserialize(serdes::node_ptr_t desernode) const {
  /////////////////////////////////////////////
  // only use this implementation for non-enums
  /////////////////////////////////////////////
  using eif = typename std::enable_if<not std::is_enum<T>::value, void>::type;
  using csd = typename std::enable_if<not use_custom_serdes<T>::enable, void>::type;
  /////////////////////////////////////////////

  auto instance   = desernode->_deser_instance;
  const auto& var = desernode->_value;
  T value;
  serdes::decode_value<T>(var, value);
  set(value, instance);
}
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ITyped<int>::deserialize(serdes::node_ptr_t desernode) const {
  auto instance   = desernode->_deser_instance;
  const auto& var = desernode->_value;
  set(int(var.get<double>()), instance);
}
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ITyped<uint32_t>::deserialize(serdes::node_ptr_t desernode) const {
  auto instance   = desernode->_deser_instance;
  const auto& var = desernode->_value;
  set(uint32_t(var.get<double>()), instance);
}
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ITyped<size_t>::deserialize(serdes::node_ptr_t desernode) const {
  auto instance   = desernode->_deser_instance;
  const auto& var = desernode->_value;
  set(size_t(var.get<double>()), instance);
}
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ITyped<object_ptr_t>::deserialize(serdes::node_ptr_t desernode) const {
  auto instance     = desernode->_deser_instance;
  auto deserializer = desernode->_deserializer;
  auto childnode    = deserializer->deserializeObject(desernode);
  if (childnode) {
    auto subinstance = childnode->_deser_instance;
    set(subinstance, instance);
  } else {
    set(object_ptr_t(nullptr), instance);
  }
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> //
void ITyped<T>::serialize(serdes::node_ptr_t leafnode) const {
  /////////////////////////////////////////////
  // only use this implementation for non-enums
  /////////////////////////////////////////////
  using eif = typename std::enable_if<not std::is_enum<T>::value>::type;
  using csd = typename std::enable_if<not use_custom_serdes<T>::enable, void>::type;
  /////////////////////////////////////////////
  auto serializer = leafnode->_serializer;
  auto instance   = leafnode->_ser_instance;
  T value;
  get(value, instance);
  leafnode->_value.template set<T>(value);
  serializer->serializeLeaf(leafnode);
}
///////////////////////////////////////////////////////////////////////////////
template <> //
inline void ITyped<object_ptr_t>::serialize(serdes::node_ptr_t propnode) const {
  auto serializer  = propnode->_serializer;
  auto parinstance = propnode->_ser_instance;
  auto nonconst    = std::const_pointer_cast<Object>(parinstance);
  object_ptr_t child_instance;
  get(child_instance, parinstance);
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
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
