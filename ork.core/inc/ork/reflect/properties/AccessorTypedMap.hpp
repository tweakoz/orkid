////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "AccessorTypedMap.h"
#include "ITypedMap.hpp"
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork { namespace reflect {

template <typename KeyType, typename ValueType>
AccessorTypedMap<KeyType, ValueType>::AccessorTypedMap(
    bool (Object::*getter)(const KeyType&, int, ValueType&) const,
    void (Object::*setter)(const KeyType&, int, const ValueType&),
    void (Object::*eraser)(const KeyType&, int))
    : mGetter(getter)
    , mSetter(setter)
    , mEraser(eraser) {
}

template <typename KeyType, typename ValueType>
bool AccessorTypedMap<KeyType, ValueType>::ReadElement(
    object_constptr_t object,
    const KeyType& key,
    int multi_index,
    ValueType& value) const {
  return (object->*mGetter)(key, multi_index, value);
}

template <typename KeyType, typename ValueType>
bool AccessorTypedMap<KeyType, ValueType>::EraseElement(object_ptr_t object, const KeyType& key, int multi_index) const {
  return (object->*mEraser)(key, multi_index);
}

template <typename KeyType, typename ValueType>
bool AccessorTypedMap<KeyType, ValueType>::WriteElement(
    object_ptr_t object,
    const KeyType& key,
    int multi_index,
    const ValueType* value) const {
  if (value) {
    (object->*mSetter)(key, multi_index, *value);
  } else {
    (object->*mEraser)(key, multi_index);
  }

  return true;
}

}} // namespace ork::reflect
