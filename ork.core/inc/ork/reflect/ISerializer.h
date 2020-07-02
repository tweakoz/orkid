////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

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
  virtual node_ptr_t serializeMapElement(node_ptr_t elemnode) {
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
  elemnode->_value.template Set<T>(inp);
  serializer->serializeMapElement(elemnode);
  serializer->popNode(); // pop elemnode
  return elemnode;
}

} // namespace ork::reflect::serdes
