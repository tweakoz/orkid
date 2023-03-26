////////////////////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
    void (Object::*serializer)(
        typename AccessorObjectMap<KeyType>::SerializationFunction, //
        serdes::BidirectionalSerializer&) const)
    : _getter(get)
    , _accessor(access)
    , _eraser(erase)
    , _serializer(serializer) {
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
object_ptr_t AccessorObjectMap<KeyType>::accessItem(
    serdes::IDeserializer& key_deserializer, //
    int multi_index,
    object_ptr_t instance) const {
  KeyType key;

  // BidirectionalSerializer(key_deserializer) | key;

  // if ((instance.get()->*_getter)(key, multi_index)) {
  // return (instance.get()->*_accessor)(key, multi_index);
  //}

  return nullptr;
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType>
object_constptr_t AccessorObjectMap<KeyType>::accessItem(
    serdes::IDeserializer& key_deserializer, //
    int multi_index,
    object_constptr_t instance) const {
  // KeyType key;
  // BidirectionalSerializer(key_deserializer) | key;
  return nullptr; //(instance.get()->*_getter)(key, multi_index);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType> void AccessorObjectMap<KeyType>::deserialize(serdes::node_ptr_t desernode) const {
  /*Command item;
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
  deserializer.endCommand(item);*/
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType> void AccessorObjectMap<KeyType>::serialize(serdes::node_ptr_t sernode) const {
  // BidirectionalSerializer bidi(serializer);
  // auto non_const = const_cast<Object*>(instance.get());
  //(non_const->*_serializer)(_serdesimpl, bidi);
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
