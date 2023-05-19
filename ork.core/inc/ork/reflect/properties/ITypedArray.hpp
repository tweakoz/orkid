////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

////////////////////////////////////////////////////////////////////////////////
template <typename elem_t> //
array_varray_t ITypedArray<elem_t>::enumerateElements(object_constptr_t instance) const {
  array_varray_t rval;
  int numelements        = count(instance);
  //printf( "map<%s> ser numelem<%d>\n", _name.c_str(), numelements );
  for (size_t i = 0; i < numelements; i++) {
    //////////////////////////////
    elem_t V;
    get(V,instance, i);
    //////////////////////////////
    array_abstract_item_t abstract;
    abstract.set<elem_t>(V);
    rval.push_back(abstract);
  }
  return rval;
}

template <typename elem_t> //
void ITypedArray<elem_t>::setElement(object_ptr_t obj,array_abstract_item_t value) const {
  const elem_t& typed_value = value.get<elem_t>();
  set(typed_value,obj,0);  
}

}} // namespace ork::reflect
