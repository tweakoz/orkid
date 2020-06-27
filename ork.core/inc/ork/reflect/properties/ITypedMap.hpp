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
  BidirectionalSerializer bidi(serializer);
  MapSerialization(_doSerialize, bidi, object);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::serializeItem(
    ISerializer& value_serializer,
    IDeserializer& key_deserializer,
    int multi_index,
    object_constptr_t object) const {
  KeyType key;
  ValueType value;
  BidirectionalSerializer(key_deserializer) | key;
  bool ok = ReadItem(object, key, multi_index, value);
  OrkAssert(ok);
  BidirectionalSerializer(value_serializer) | value;
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::_doSerialize(
    BidirectionalSerializer& bidi, //
    KeyType& key,
    ValueType& value) {
  ISerializer* serializer = bidi.Serializer();
  Command item(Command::EITEM);
  Command attribute(Command::EATTRIBUTE, "key");
  serializer->beginCommand(item);
  serializer->beginCommand(attribute);
  serializer->Hint("map_key", key);
  serializer->endCommand(attribute);
  serializer->Hint("map_value", value);
  bidi | value;
  serializer->endCommand(item);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::deserialize(
    IDeserializer& deserializer, //
    object_ptr_t object) const {
  BidirectionalSerializer bidi(deserializer);
  KeyType key;
  ValueType value;
  size_t numitems = itemCount(object);
  for (size_t i = 0; i < numitems; i++) {
    _doDeserialize(bidi, key, value);
    WriteItem(object, key, -1, &value);
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::deserializeItem(
    IDeserializer* value_deserializer,
    IDeserializer& key_deserializer,
    int multi_index,
    object_ptr_t object) const {
  KeyType key;
  ValueType value;
  BidirectionalSerializer(key_deserializer) | key;
  if (value_deserializer) {
    BidirectionalSerializer(*value_deserializer) | value;
    WriteItem(object, key, multi_index, &value);
  } else {
    WriteItem(object, key, multi_index, nullptr);
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::_doDeserialize(
    BidirectionalSerializer& bidi, //
    KeyType& key,
    ValueType& value) {
  IDeserializer* deserializer = bidi.Deserializer();
  Command item;
  deserializer->beginCommand(item);
  OrkAssert(item.Type() == Command::EITEM);
  deserializer->endCommand(item);

  Command attribute;
  deserializer->beginCommand(attribute);

  OrkAssert(attribute.Type() == Command::EATTRIBUTE);
  OrkAssert(attribute.Name() == "key");

  deserializer->Hint("map_key");
  bidi | key;
  deserializer->endCommand(attribute);

  deserializer->Hint("map_value");
  bidi | value;
  deserializer->endCommand(item);
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
