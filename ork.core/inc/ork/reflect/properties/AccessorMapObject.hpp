////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/AccessorMapObject.h>

#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/object/Object.h>

namespace ork { namespace reflect {

template <typename KeyType>
AccessorMapObject<KeyType>::AccessorMapObject(
    const Object* (Object::*get)(const KeyType&, int) const,
    Object* (Object::*access)(const KeyType&, int),
    void (Object::*erase)(const KeyType&, int),
    void (Object::*serializer)(typename AccessorMapObject<KeyType>::SerializationFunction, BidirectionalSerializer&)
        const)
    : mGetter(get)
    , mAccessor(access)
    , mEraser(erase)
    , mSerializer(serializer) {
}

template <typename KeyType>
Object*
AccessorMapObject<KeyType>::AccessItem(IDeserializer& key_deserializer, int multi_index, Object* object) const {
  KeyType key;

  BidirectionalSerializer(key_deserializer) | key;

  if ((object->*mGetter)(key, multi_index)) {
    return (object->*mAccessor)(key, multi_index);
  }

  return NULL;
}

template <typename KeyType>
const Object*
AccessorMapObject<KeyType>::AccessItem(IDeserializer& key_deserializer, int multi_index, const Object* object) const {
  KeyType key;

  BidirectionalSerializer(key_deserializer) | key;

  return (object->*mGetter)(key, multi_index);
}

template <typename KeyType>
bool AccessorMapObject<KeyType>::DeserializeItem(
    IDeserializer* value_deserializer,
    IDeserializer& key_deserializer,
    int multi_index,
    Object* object) const {
  bool result = true;

  KeyType key;

  BidirectionalSerializer(key_deserializer) | key;

  if (value_deserializer) {
    Object* value = (object->*mAccessor)(key, multi_index);

    if (value) {
      result = Object::xxxDeserialize(value, *value_deserializer);
    } else {
      result = false;
    }
  } else {
    (object->*mEraser)(key, multi_index);
  }

  return result;
}

template <typename KeyType>
bool AccessorMapObject<KeyType>::SerializeItem(
    ISerializer& value_serializer,
    IDeserializer& key_deserializer,
    int multi_index,
    const Object* object) const {
  KeyType key;

  BidirectionalSerializer(key_deserializer) | key;

  const Object* value = (object->*mGetter)(key, multi_index);

  if (value) {
    return Object::xxxSerialize(value, value_serializer);
  } else {
    return false;
  }
}

template <typename KeyType>
bool AccessorMapObject<KeyType>::Deserialize(IDeserializer& deserializer, Object* object) const {
  Command item;

  if (deserializer.BeginCommand(item)) {
    OrkAssert(item.Type() == Command::EITEM);

    if (item.Type() != Command::EITEM) {
      deserializer.EndCommand(item);
      return false;
    }

    Command attribute;
    if (false == deserializer.BeginCommand(attribute))
      return false;

    OrkAssert(attribute.Type() == Command::EATTRIBUTE);
    OrkAssert(attribute.Name() == "key");

    if (attribute.Type() != Command::EATTRIBUTE || attribute.Name() != "key") {
      deserializer.EndCommand(attribute);
      return false;
    }

    KeyType key;

    BidirectionalSerializer(deserializer) | key;

    if (false == deserializer.EndCommand(attribute))
      return false;

    Object* value = (object->*mAccessor)(key, IObjectMapProperty::kDeserializeInsertItem);

    if (false == Object::xxxDeserialize(value, deserializer))
      return false;

    if (false == deserializer.EndCommand(item))
      return false;

    return true;
  }

  return false;
}

template <typename KeyType>
bool AccessorMapObject<KeyType>::Serialize(ISerializer& serializer, const Object* obj) const {
  BidirectionalSerializer bidi(serializer);
  (obj->*mSerializer)(DoSerialize, bidi);

  return bidi.Succeeded();
}

template <typename KeyType>
void AccessorMapObject<KeyType>::DoSerialize(BidirectionalSerializer& bidi, const KeyType& key, const Object* value) {
  bool result             = true;
  ISerializer* serializer = bidi.Serializer();

  Command item(Command::EITEM);

  Command attribute(Command::EATTRIBUTE, "key");

  if (false == serializer->BeginCommand(item))
    result = false;
  if (false == serializer->BeginCommand(attribute))
    result = false;
  bidi | key;
  if (false == serializer->EndCommand(attribute))
    result = false;

  if (false == Object::xxxSerialize(value, *bidi.Serializer()))
    result = false;

  if (false == serializer->EndCommand(item))
    result = false;

  if (false == result)
    bidi.Fail();
}

}} // namespace ork::reflect
