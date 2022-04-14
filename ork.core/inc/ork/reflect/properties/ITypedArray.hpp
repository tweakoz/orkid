////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedArray.h"
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include "codec.inl"

namespace ork { namespace reflect {

template <typename elem_t> //
void ITypedArray<elem_t>::deserializeElement(serdes::node_ptr_t arynode) const {
  auto deserializer = arynode->_deserializer;
  auto instance     = arynode->_deser_instance;
  int index         = arynode->_index;
  auto elemnode     = deserializer->pushNode("", serdes::NodeType::ARRAY_ELEMENT_LEAF);
  elemnode->_parent = arynode;
  auto childnode    = deserializer->deserializeElement(elemnode);
  deserializer->popNode();
  if constexpr (std::is_convertible<elem_t, object_ptr_t>::value) {
    object_ptr_t objvalue;
    serdes::decode_value<object_ptr_t>(childnode->_value, objvalue);
    using ptrtype_t      = typename elem_t::element_type;
    auto typed_ptr_value = std::dynamic_pointer_cast<ptrtype_t>(objvalue);
    set(typed_ptr_value, instance, index);
  } else {
    elem_t value;
    serdes::decode_value<elem_t>(childnode->_value, value);
    set(value, instance, index);
  }
}

template <typename elem_t> //
void ITypedArray<elem_t>::serializeElement(serdes::node_ptr_t elemnode) const {
  elem_t value;
  auto serializer = elemnode->_serializer;
  auto instance   = elemnode->_ser_instance;
  get(value, instance, elemnode->_index);
  if constexpr (std::is_convertible<elem_t, object_ptr_t>::value) {
    elemnode->_value.template set<object_ptr_t>(value);
  } else {
    elemnode->_value.template set<elem_t>(value);
  }
  auto childnode = serializer->serializeContainerElement(elemnode);
}

}} // namespace ork::reflect
