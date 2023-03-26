////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "serialize/serdes.h"
#include "properties/codec.h"
#include <stack>

namespace ork::reflect {
struct ObjectProperty;
}
namespace ork::reflect::serdes {

class Command;

struct IDeserializer {

  virtual node_ptr_t pushNode(std::string named, NodeType type);
  virtual void popNode();
  virtual void deserializeTop(object_ptr_t&) = 0;
  virtual node_ptr_t deserializeObject(node_ptr_t);
  virtual node_ptr_t deserializeElement(node_ptr_t elemnode);

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
    int index);

template <typename T>
T deserializeMapSubLeaf(
    serdes::node_ptr_t mapnode, //
    std::string& key_out);

} // namespace ork::reflect::serdes
