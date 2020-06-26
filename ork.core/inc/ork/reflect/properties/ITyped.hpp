////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITyped.h"
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>

namespace ork { namespace reflect {

template <typename T> void ITyped<T>::deserialize(IDeserializer& deserializer, object_ptr_t obj) const {
  T value;
  BidirectionalSerializer bidi(deserializer);
  bidi | value;
  set(value, obj);
}

template <typename T> void ITyped<T>::serialize(ISerializer& serializer, object_constptr_t obj) const {
  T value;
  get(value, obj);
  BidirectionalSerializer bidi(serializer);
  bidi | value;
}

}} // namespace ork::reflect
