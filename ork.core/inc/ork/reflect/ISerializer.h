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

  void referenceObject(object_constptr_t);

  virtual node_ptr_t serializeTop(object_constptr_t) {
    return node_ptr_t(nullptr);
  }
  virtual node_ptr_t serializeElement(node_ptr_t elemnode) {
    return node_ptr_t(nullptr);
  }

  virtual ~ISerializer();

  std::unordered_set<std::string> _reftracker;

  struct Node {
    node_ptr_t _parent                       = nullptr;
    const reflect::ObjectProperty* _property = nullptr;
    ISerializer* _serializer                 = nullptr;
    object_constptr_t _instance              = nullptr;
    var_t _impl;
    std::string _key;
    var_t _value;
    size_t _index       = 0;
    size_t _multiindex  = 0;
    size_t _numchildren = 0;
  };
};
}} // namespace ork::reflect
