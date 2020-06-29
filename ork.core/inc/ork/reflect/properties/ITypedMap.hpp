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
#include "codec.inl"

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
  Command element(Command::ELEMENT);
  Command attribute(Command::EATTRIBUTE, "key");
  ser.beginCommand(element);
  ser.beginCommand(attribute);
  ser.Hint("map_key", key);
  ser.endCommand(attribute);
  ser.Hint("map_value", ValueType(value));
  ser.endCommand(element);
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::deserialize(IDeserializer::node_ptr_t dsernode) const {
  KeyType key;
  ValueType value;
  auto deserializer  = dsernode->_deserializer;
  size_t numelements = dsernode->_numchildren;
  auto elemnode      = std::make_shared<IDeserializer::Node>();
  elemnode->_parent  = dsernode;
  auto instance      = dsernode->_instance;
  for (size_t i = 0; i < numelements; i++) {
    dsernode->_index = i;
    auto childnode   = deserializer->deserializeElement(dsernode);
    decode_key<KeyType>(childnode->_key, key);
    decode_value<ValueType>(childnode->_value, value);
    this->WriteElement(
        instance, //
        key,
        -1,
        &value);
  }
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
