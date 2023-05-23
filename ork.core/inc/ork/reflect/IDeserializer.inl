////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "IDeserializer.h"
#include "properties/codec.inl"
#include <ork/kernel/prop.h>

namespace ork::reflect::serdes {

template <typename T>
T deserializeArraySubLeaf(
    serdes::node_ptr_t arynode, //
    int index) {
  auto deserializer = arynode->_deserializer;
  auto elemnode     = deserializer->pushNode("", serdes::NodeType::ARRAY_ELEMENT_LEAF);
  elemnode->_parent = arynode;
  auto childnode    = deserializer->deserializeElement(elemnode);
  deserializer->popNode();
  T value;
  serdes::decode_value<T>(childnode->_value, value);
  return value;
}

template <typename T>
T deserializeMapSubLeaf(
    serdes::node_ptr_t mapnode, //
    std::string& key_out) {
  auto deserializer = mapnode->_deserializer;
  auto elemnode     = deserializer->pushNode("", serdes::NodeType::MAP_ELEMENT_LEAF);
  elemnode->_parent = mapnode;
  auto childnode    = deserializer->deserializeElement(elemnode);
  deserializer->popNode();
  T value;
  serdes::decode_value<T>(childnode->_value, value);
  key_out = childnode->_key;
  return value;
}

inline serdes::node_ptr_t fetchNextMapSubLeaf(serdes::node_ptr_t mapnode) {
  auto deserializer = mapnode->_deserializer;
  auto elemnode     = deserializer->pushNode("", serdes::NodeType::MAP_ELEMENT_LEAF);
  elemnode->_parent = mapnode;
  auto childnode    = deserializer->deserializeElement(elemnode);
  deserializer->popNode();
  return childnode;
}

} // namespace ork::reflect::serdes
