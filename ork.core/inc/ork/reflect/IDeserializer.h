////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "serialize/serdes.h"
#include "properties/codec.inl"
#include <stack>

namespace ork::reflect {
class ObjectProperty;
}
namespace ork::reflect::serdes {

class Command;

struct IDeserializer {

  virtual node_ptr_t pushNode(std::string named, NodeType type) {
    return nullptr;
  }
  virtual void popNode() {
    return;
  }

  virtual void deserializeTop(object_ptr_t&) = 0;
  virtual node_ptr_t deserializeObject(node_ptr_t) {
    return node_ptr_t(nullptr);
  }
  virtual node_ptr_t deserializeElement(node_ptr_t elemnode) {
    return node_ptr_t(nullptr);
  }

  void trackObject(boost::uuids::uuid id, object_ptr_t instance);
  object_ptr_t findTrackedObject(boost::uuids::uuid id) const;
  virtual ~IDeserializer();

  ///////////////////////////////////////////

  using trackervect_t = std::unordered_map<std::string, object_ptr_t>;
  std::stack<node_ptr_t> _nodestack;
  trackervect_t _reftracker;
};

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

} // namespace ork::reflect::serdes
