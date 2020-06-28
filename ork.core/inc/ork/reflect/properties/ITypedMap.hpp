////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedMap.h"
#include <ork/reflect/Command.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork::reflect {
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::serialize(
    ISerializer& serializer, //
    object_constptr_t object) const {
  // BidirectionalSerializer bidi(serializer);
  // MapSerialization(_doSerialize, bidi, object);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::_doSerialize(
    ISerializer& ser, //
    const KeyType& key,
    const ValueType& value) {
  Command item(Command::EITEM);
  Command attribute(Command::EATTRIBUTE, "key");
  ser.beginCommand(item);
  ser.beginCommand(attribute);
  ser.Hint("map_key", key);
  ser.endCommand(attribute);
  ser.Hint("map_value", ValueType(value));
  ser.endCommand(item);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::deserialize(
    IDeserializer& deserializer, //
    object_ptr_t object) const {
  KeyType key;
  ValueType value;
  size_t numitems = itemCount(object);
  for (size_t i = 0; i < numitems; i++) {
    _doDeserialize(deserializer, key, value);
    WriteItem(object, key, -1, &value);
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::_doDeserialize(
    IDeserializer& dser, //
    KeyType& key,
    ValueType& value) {
  Command item;
  dser.beginCommand(item);
  OrkAssert(item.Type() == Command::EITEM);
  dser.endCommand(item);

  Command attribute;
  dser.beginCommand(attribute);

  OrkAssert(attribute.Type() == Command::EATTRIBUTE);
  OrkAssert(attribute.Name() == "key");

  dser.Hint("map_key");
  dser.deserializeItem();
  // bidi | key;
  dser.endCommand(attribute);

  dser.Hint("map_value");
  dser.deserializeItem();
  dser.endCommand(item);
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
