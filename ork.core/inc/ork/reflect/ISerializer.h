////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/svariant.h>
#include <stdint.h>
#include <unordered_set>
#include <boost/uuid/uuid.hpp>

namespace ork { namespace rtti {
class ICastable;
}} // namespace ork::rtti

namespace ork { namespace reflect {

class ObjectProperty;
class IArray;

struct ISerializer {
public:
  using var_t = svar1024_t;
  struct Node;
  using node_ptr_t = std::shared_ptr<Node>;

  node_ptr_t topNode();
  node_ptr_t serializeRoot(object_constptr_t);

  virtual node_ptr_t pushNode(std::string named) {
    return nullptr;
  }
  virtual void popNode() {
  }
  virtual node_ptr_t serializeObject(node_ptr_t parnode) {
    return node_ptr_t(nullptr);
  }
  virtual node_ptr_t serializeElement(node_ptr_t elemnode) {
    return node_ptr_t(nullptr);
  }
  virtual void serializeLeaf(node_ptr_t leafnode) {
    return;
  }

  virtual ~ISerializer();

  std::unordered_set<std::string> _reftracker;
  std::stack<node_ptr_t> _nodestack;
  node_ptr_t _rootnode;

  struct Node {
    node_ptr_t _parent                       = nullptr;
    const reflect::ObjectProperty* _property = nullptr;
    ISerializer* _serializer                 = nullptr;
    object_constptr_t _instance              = nullptr;
    var_t _impl;
    std::string _key;
    std::string _name;
    var_t _value;
    size_t _index       = 0;
    size_t _multiindex  = 0;
    size_t _numchildren = 0;
    bool _isobject      = false;
  };
};
}} // namespace ork::reflect
