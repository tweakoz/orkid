////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/svariant.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/MutableString.h>
#include <ork/orktypes.h>
#include <unordered_map>
#include <boost/uuid/uuid.hpp>

namespace ork::reflect {

class ObjectProperty;
class Command;

struct IDeserializer {

  using var_t = svar64_t;
  struct Node;
  using node_ptr_t = std::shared_ptr<Node>;

  virtual void deserializeTop(object_ptr_t&) = 0;
  virtual node_ptr_t deserializeElement(node_ptr_t elemnode) {
    return node_ptr_t(nullptr);
  }

  void trackObject(boost::uuids::uuid id, object_ptr_t instance);
  object_ptr_t findTrackedObject(boost::uuids::uuid id) const;
  virtual ~IDeserializer();

  ///////////////////////////////////////////

  struct Node {
    node_ptr_t _parent                       = nullptr;
    const reflect::ObjectProperty* _property = nullptr;
    IDeserializer* _deserializer             = nullptr;
    object_ptr_t _instance                   = nullptr;
    var_t _impl;
    std::string _key;
    var_t _value;
    size_t _index       = -1;
    size_t _numchildren = 0;
  };

  ///////////////////////////////////////////

  using trackervect_t = std::unordered_map<std::string, object_ptr_t>;
  trackervect_t _reftracker;
};

} // namespace ork::reflect
