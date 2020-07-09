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
template <typename kt, typename vt> bool IsMultiMapDeducer(const std::map<kt, vt>& map) {
  return false;
}
template <typename kt, typename vt> bool IsMultiMapDeducer(const std::unordered_map<kt, vt>& map) {
  return false;
}

template <typename kt, typename vt> bool IsMultiMapDeducer(const std::multimap<kt, vt>& map) {
  return true;
}

template <typename kt, typename vt> bool IsMultiMapDeducer(const ork::orklut<kt, vt>& map) {
  return map.GetKeyPolicy() == ork::EKEYPOLICY_MULTILUT;
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType> //
void ITypedMap<KeyType, ValueType>::serialize(serdes::node_ptr_t sernode) const {
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto mapnode           = serializer->pushNode(_name, serdes::NodeType::MAP);
  mapnode->_parent       = sernode;
  mapnode->_ser_instance = instance;
  int numelements        = elementCount(instance);
  for (size_t i = 0; i < numelements; i++) {
    //////////////////////////////
    KeyType K;
    ValueType V;
    GetKey(instance, i, K);
    GetVal(instance, K, V);
    //////////////////////////////
    std::string keystr;
    serdes::encode_key(keystr, K);
    //////////////////////////////
    auto elemnode = serializer->pushNode(keystr, serdes::NodeType::MAP_ELEMENT_LEAF);
    //////////////////////////////
    elemnode->_key = keystr;
    elemnode->_value.template Set<ValueType>(V);
    elemnode->_index        = i;
    elemnode->_parent       = mapnode;
    elemnode->_ser_instance = instance;
    elemnode->_serializer   = serializer;
    auto childnode          = serializer->serializeContainerElement(elemnode);
    //////////////////////////////
    serializer->popNode(); // pop elemnode
    //////////////////////////////
  }
  serializer->popNode(); // pop mapnode
}
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType> //
void ITypedMap<KeyType, ValueType>::deserialize(serdes::node_ptr_t dsernode) const {
  KeyType key;
  ValueType value;
  auto deserializer  = dsernode->_deserializer;
  size_t numelements = dsernode->_numchildren;
  auto instance      = dsernode->_deser_instance;
  for (size_t i = 0; i < numelements; i++) {
    dsernode->_index  = i;
    auto elemnode     = deserializer->pushNode("", serdes::NodeType::MAP_ELEMENT_LEAF);
    elemnode->_parent = dsernode;
    auto childnode    = deserializer->deserializeElement(elemnode);
    serdes::decode_key<KeyType>(childnode->_key, key);
    childnode->_name = childnode->_key;
    serdes::decode_value<ValueType>(childnode->_value, value);
    this->WriteElement(
        instance, //
        key,
        -1,
        &value);
    deserializer->popNode();
  }
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
