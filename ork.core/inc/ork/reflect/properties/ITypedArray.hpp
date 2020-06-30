////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedArray.h"
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork { namespace reflect {

template <typename T> //
void ITypedArray<T>::deserializeElement(serdes::node_ptr_t desernode) const {
  // BidirectionalSerializer bidi(deserializer);
  // T value;
  // bidi | value;
  // set(value, obj, index);
}

template <typename T> //
void ITypedArray<T>::serializeElement(serdes::node_ptr_t sernode) const {
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto arynode           = serializer->pushNode(_name, serdes::NodeType::ARRAY);
  arynode->_parent       = sernode;
  arynode->_ser_instance = instance;
  int numelements        = count(instance);
  for (size_t i = 0; i < numelements; i++) {
  }
  serializer->popNode(); // pop mapnode
}

}} // namespace ork::reflect
