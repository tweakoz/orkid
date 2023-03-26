////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stack>
#include "types.h"
#include "serialize/serdes.h"

namespace ork::reflect::serdes {

struct ISerializer {
public:
  node_ptr_t topNode();
  node_ptr_t serializeRoot(object_constptr_t);

  virtual node_ptr_t pushNode(std::string named, NodeType type) {
    return nullptr;
  }
  virtual void popNode() {
  }
  virtual node_ptr_t serializeObject(node_ptr_t parnode) {
    return node_ptr_t(nullptr);
  }
  virtual node_ptr_t serializeContainerElement(node_ptr_t elemnode) {
    return node_ptr_t(nullptr);
  }
  virtual void serializeLeaf(node_ptr_t leafnode) {
    return;
  }

  virtual ~ISerializer();

  std::unordered_set<std::string> _reftracker;
  std::stack<node_ptr_t> _nodestack;
  node_ptr_t _rootnode;
};

template <typename T>
serdes::node_ptr_t serializeArraySubLeaf(
    serdes::node_ptr_t arynode, //
    T inp,
    int index) {
  auto serializer         = arynode->_serializer;
  auto instance           = arynode->_ser_instance;
  auto elemnode           = serializer->pushNode("aryelem", serdes::NodeType::ARRAY_ELEMENT_LEAF);
  elemnode->_index        = index;
  elemnode->_parent       = arynode;
  elemnode->_ser_instance = instance;
  elemnode->_serializer   = serializer;
  elemnode->_value.template set<T>(inp);
  serializer->serializeContainerElement(elemnode);
  serializer->popNode(); // pop elemnode
  return elemnode;
}

template <typename T>
serdes::node_ptr_t serializeMapSubLeaf(
    serdes::node_ptr_t mapnode, //
    std::string key,
    T inp) {
  auto serializer = mapnode->_serializer;
  auto instance   = mapnode->_ser_instance;
  auto elemnode   = serializer->pushNode(key, serdes::NodeType::MAP_ELEMENT_LEAF);
  elemnode->_key  = key;
  elemnode->_value.template set<T>(inp);
  elemnode->_index        = 0;
  elemnode->_parent       = mapnode;
  elemnode->_ser_instance = instance;
  elemnode->_serializer   = serializer;
  auto childnode          = serializer->serializeContainerElement(elemnode);
  serializer->popNode(); // pop element node
  return elemnode;
} // namespace ork::reflect::serdes

} // namespace ork::reflect::serdes
