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
template <typename T> //
inline void decode_key(std::string keystr, T& key_out) {
  OrkAssert(false);
}
template <> //
inline void decode_key(std::string keystr, int& key_out) {
  OrkAssert(false);
}
template <> //
inline void decode_key(std::string keystr, std::string& key_out) {
  key_out = keystr;
}
template <typename T> //
inline void decode_value(IDeserializer::var_t val_inp, T& val_out) {
  val_out = val_inp.Get<T>();
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
/*template <typename KeyType> //
void ITypedMap<KeyType, object_ptr_t>::deserialize(IDeserializer::node_ptr_t dsernode) const {
  KeyType key;
  object_ptr_t value;
  auto deserializer  = dsernode->_deserializer;
  size_t numelements = dsernode->_numchildren;
  auto elemnode      = std::make_shared<IDeserializer::Node>();
  elemnode->_parent  = dsernode;
  auto instance      = dsernode->_instance;
  for (size_t i = 0; i < numelements; i++) {
    elemnode->_index = i;
    deserializer->deserializeElement(elemnode);
    const auto& key = elemnode->_key;
    dsernode->WriteElement(
        instance, //
        key.c_str(),
        -1,
        &instance);
  }
}*.
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType>
void ITypedMap<KeyType, ValueType>::_doDeserialize(
    IDeserializer& dser, //
    KeyType& key,
    ValueType& value) {
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
dser.endCommand(element);
} // namespace ork::reflect
////////////////////////////////////////////////////////////////////////////////
*/
} // namespace ork::reflect
