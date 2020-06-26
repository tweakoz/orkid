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

template <typename T> bool ITyped<T>::Deserialize(IDeserializer& deserializer, Object* obj) const {
  T value;

  BidirectionalSerializer bidi(deserializer);

  bidi | value;

  bool result = bidi.Succeeded();

  if (result) {
    Set(value, obj);
  }

  return result;
}

template <typename T> bool ITyped<T>::Serialize(ISerializer& serializer, object_constptr_t) const {
  T value;
  Get(value, obj);

  BidirectionalSerializer bidi(serializer);

  bidi | value;

  return bidi.Succeeded();
}

template <typename T> typename ITyped<T>::RTTITyped::RTTICategory ITyped<T>::sClass(ITyped<T>::RTTITyped::ClassRTTI());

template <typename T> typename ITyped<T>::RTTITyped::RTTICategory* ITyped<T>::GetClassStatic() {
  return &sClass;
}

template <typename T> typename ITyped<T>::RTTITyped::RTTICategory* ITyped<T>::GetClass() const {
  return GetClassStatic();
}

}} // namespace ork::reflect
