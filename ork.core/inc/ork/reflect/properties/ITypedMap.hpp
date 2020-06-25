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

namespace ork { namespace reflect {

template <typename KeyType, typename ValueType>
bool ITypedMap<KeyType, ValueType>::DeserializeItem(
    IDeserializer* value_deserializer,
    IDeserializer& key_deserializer,
    int multi_index,
    Object* object) const {
  KeyType key;
  ValueType value;

  BidirectionalSerializer(key_deserializer) | key;

  if (value_deserializer) {
    BidirectionalSerializer(*value_deserializer) | value;
    WriteItem(object, key, multi_index, &value);
  } else {
    WriteItem(object, key, multi_index, NULL);
  }

  return true;
}

template <typename KeyType, typename ValueType>
bool ITypedMap<KeyType, ValueType>::SerializeItem(
    ISerializer& value_serializer,
    IDeserializer& key_deserializer,
    int multi_index,
    const Object* object) const {
  KeyType key;
  ValueType value;

  BidirectionalSerializer(key_deserializer) | key;

  if (ReadItem(object, key, multi_index, value)) {
    BidirectionalSerializer(value_serializer) | value;
    return true;
  } else {
    return false;
  }
}

template <typename KeyType, typename ValueType>
bool ITypedMap<KeyType, ValueType>::Deserialize(IDeserializer& deserializer, Object* object) const {
  BidirectionalSerializer bidi(deserializer);

  KeyType key;
  ValueType value;

  while (DoDeserialize(bidi, key, value)) {
    WriteItem(object, key, -1, &value);
  }

  return bidi.Succeeded();
}

template <typename KeyType, typename ValueType>
bool ITypedMap<KeyType, ValueType>::Serialize(ISerializer& serializer, const Object* object) const {
  BidirectionalSerializer bidi(serializer);

  MapSerialization(DoSerialize, bidi, object);

  return bidi.Succeeded();
}

template <typename KeyType, typename ValueType>
bool ITypedMap<KeyType, ValueType>::DoDeserialize(BidirectionalSerializer& bidi, KeyType& key, ValueType& value) {
  IDeserializer* deserializer = bidi.Deserializer();

  Command item;

  if (deserializer->BeginCommand(item)) {
    OrkAssert(item.Type() == Command::EITEM);

    if (item.Type() != Command::EITEM) {
      deserializer->EndCommand(item);
      bidi.Fail();
      return false;
    }

    Command attribute;
    deserializer->BeginCommand(attribute);

    OrkAssert(attribute.Type() == Command::EATTRIBUTE);
    OrkAssert(attribute.Name() == "key");

    if (attribute.Type() != Command::EATTRIBUTE || attribute.Name() != "key") {
      deserializer->EndCommand(attribute);
      bidi.Fail();
      return false;
    }

    deserializer->Hint("map_key");
    bidi | key;
    deserializer->EndCommand(attribute);

    deserializer->Hint("map_value");
    bidi | value;
    deserializer->EndCommand(item);

    return true;
  }

  return false;
}

template <typename KeyType, typename ValueType>
bool ITypedMap<KeyType, ValueType>::DoSerialize(BidirectionalSerializer& bidi, KeyType& key, ValueType& value) {
  bool result             = true;
  ISerializer* serializer = bidi.Serializer();

  Command item(Command::EITEM);

  Command attribute(Command::EATTRIBUTE, "key");

  if (false == serializer->BeginCommand(item))
    result = false;
  if (false == serializer->BeginCommand(attribute))
    result = false;

  serializer->Hint("map_key");
  bidi | key;
  if (false == serializer->EndCommand(attribute))
    result = false;

  serializer->Hint("map_value");
  bidi | value;
  if (false == serializer->EndCommand(item))
    result = false;

  if (false == result)
    bidi.Fail();

  return bidi.Succeeded();
}

template <typename KeyType, typename ValueType>
typename ITypedMap<KeyType, ValueType>::RTTITyped::RTTICategory
    ITypedMap<KeyType, ValueType>::sClass(ITypedMap<KeyType, ValueType>::RTTITyped::ClassRTTI());

template <typename KeyType, typename ValueType>
typename ITypedMap<KeyType, ValueType>::RTTITyped::RTTICategory* ITypedMap<KeyType, ValueType>::GetClassStatic() {
  return &sClass;
}

template <typename KeyType, typename ValueType>
typename ITypedMap<KeyType, ValueType>::RTTITyped::RTTICategory* ITypedMap<KeyType, ValueType>::GetClass() const {
  return GetClassStatic();
}
}} // namespace ork::reflect
