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

template <typename elem_t> //
void ITypedArray<elem_t>::deserializeElement(serdes::node_ptr_t desernode) const {
  // BidirectionalSerializer bidi(deserializer);
  // T value;
  // bidi | value;
  // set(value, obj, index);
}

template <typename elem_t> //
void ITypedArray<elem_t>::serializeElement(serdes::node_ptr_t elemnode) const {
  elem_t value;
  auto serializer = elemnode->_serializer;
  auto instance   = elemnode->_ser_instance;
  get(value, instance, elemnode->_index);
  elemnode->_value.template Set<elem_t>(value);
  auto childnode = serializer->serializeMapElement(elemnode);
}

}} // namespace ork::reflect
