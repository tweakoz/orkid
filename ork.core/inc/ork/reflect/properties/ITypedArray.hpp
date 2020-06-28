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
void ITypedArray<T>::deserializeElement(IDeserializer::node_ptr_t desernode) const {
  // BidirectionalSerializer bidi(deserializer);
  // T value;
  // bidi | value;
  // set(value, obj, index);
}

template <typename T> //
void ITypedArray<T>::serializeItem(
    ISerializer& serializer, //
    object_constptr_t obj,
    size_t index) const {
  // BidirectionalSerializer bidi(serializer);
  // T value;
  // get(value, obj, index);
  // bidi | value;
}

}} // namespace ork::reflect
