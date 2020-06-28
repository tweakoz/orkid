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

  for (size_t i = 0; i < numelements; i++) {
    dsernode->_index = i;
    deserializer->deserializeElement(dsernode);
    //_doDeserialize(deserializer, key, value);
    // WriteElement(object, key, -1, &value);
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::_doDeserialize(
    IDeserializer& dser, //
    KeyType& key,
    ValueType& value) {
  /*
Command element;
dser.beginCommand(element);
OrkAssert(element.Type() == Command::ELEMENT);
dser.endCommand(element);

Command attribute;
dser.beginCommand(attribute);

OrkAssert(attribute.Type() == Command::EATTRIBUTE);
OrkAssert(attribute.Name() == "key");

dser.Hint("map_key");
// dser.deserializeItem();
// bidi | key;
dser.endCommand(attribute);

dser.Hint("map_value");
// dser.deserializeItem();
dser.endCommand(element);*/
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
