////////////////////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "AccessorObjectMap.h"

#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/object/Object.h>

namespace ork::reflect {
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
AccessorObjectMap<KeyType>::AccessorObjectMap(
    object_constptr_t (Object::*get)(const KeyType&, int) const,
    object_ptr_t (Object::*access)(const KeyType&, int),
    void (Object::*erase)(const KeyType&, int),
    void (Object::*serializer)(typename AccessorObjectMap<KeyType>::SerializationFunction, BidirectionalSerializer&) const)
    : _getter(get)
    , _accessor(access)
    , _eraser(erase)
    , _serializer(serializer) {
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
object_ptr_t AccessorObjectMap<KeyType>::accessItem(
    IDeserializer& key_deserializer, //
    int multi_index,
    object_ptr_t instance) const {
  KeyType key;

  BidirectionalSerializer(key_deserializer) | key;

  if ((instance.get()->*_getter)(key, multi_index)) {
    return (instance.get()->*_accessor)(key, multi_index);
  }

  return NULL;
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
object_constptr_t AccessorObjectMap<KeyType>::accessItem(
    IDeserializer& key_deserializer, //
    int multi_index,
    object_constptr_t instance) const {
  KeyType key;

  BidirectionalSerializer(key_deserializer) | key;

  return (instance.get()->*_getter)(key, multi_index);
}
////////////////////////////////////////////////////////////////////////////////
/*template <typename KeyType>
void AccessorObjectMap<KeyType>::deserializeItem(
    IDeserializer* value_deserializer,
    IDeserializer& key_deserializer,
    int multi_index,
    object_ptr_t instance) const {
  KeyType key;
  BidirectionalSerializer(key_deserializer) | key;
  if (value_deserializer) {
    object_ptr_t value = (instance.get()->*_accessor)(key, multi_index);

    if (value) {
      Object::xxxDeserializeShared(value, *value_deserializer);
    }
  } else {
    (instance.get()->*_eraser)(key, multi_index);
  }
}*/
////////////////////////////////////////////////////////////////////////////////
/*template <typename KeyType>
void AccessorObjectMap<KeyType>::serializeItem(
    ISerializer& value_serializer,
    IDeserializer& key_deserializer,
    int multi_index,
    object_constptr_t instance) const {
  KeyType key;
  BidirectionalSerializer(key_deserializer) | key;
  object_constptr_t value = (instance.get()->*_getter)(key, multi_index);
  if (value) {
    Object::xxxSerializeShared(value, value_serializer);
  }
}*/
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
void AccessorObjectMap<KeyType>::deserialize(
    IDeserializer& deserializer, //
    object_ptr_t instance) const {
  Command item;
  deserializer.beginCommand(item);
  OrkAssert(item.Type() == Command::ELEMENT);
  Command attribute;
  deserializer.beginCommand(attribute);
  OrkAssert(attribute.Type() == Command::EATTRIBUTE);
  OrkAssert(attribute.Name() == "key");
  KeyType key;
  BidirectionalSerializer(deserializer) | key;
  deserializer.endCommand(attribute);
  object_ptr_t value = (instance.get()->*_accessor)(key, IMap::kDeserializeInsertElement);
  Object::xxxDeserializeShared(value, deserializer);
  deserializer.endCommand(item);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
void AccessorObjectMap<KeyType>::serialize(
    ISerializer& serializer, //
    object_constptr_t instance) const {
  BidirectionalSerializer bidi(serializer);
  auto non_const = const_cast<Object*>(instance.get());
  (non_const->*_serializer)(_serdesimpl, bidi);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
void AccessorObjectMap<KeyType>::_serdesimpl(
    BidirectionalSerializer& bidi, //
    const KeyType& key,
    object_constptr_t value) {
  bool result             = true;
  ISerializer* serializer = bidi.Serializer();
  Command item(Command::ELEMENT);
  Command attribute(Command::EATTRIBUTE, "key");
  serializer->beginCommand(item);
  serializer->beginCommand(attribute);
  bidi | key;
  serializer->endCommand(attribute);
  Object::xxxSerializeShared(value, *bidi.Serializer());
  serializer->endCommand(item);
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
