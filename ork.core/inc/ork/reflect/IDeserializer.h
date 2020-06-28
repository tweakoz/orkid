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

  virtual void deserializeTop(object_ptr_t&) = 0;

  void trackObject(boost::uuids::uuid id, object_ptr_t instance);
  object_ptr_t findTrackedObject(boost::uuids::uuid id) const;
  virtual ~IDeserializer();

  struct Node;
  using node_ptr_t = std::shared_ptr<Node>;

  ///////////////////////////////////////////

  struct Node {
    node_ptr_t _parent                       = nullptr;
    const reflect::ObjectProperty* _property = nullptr;
    IDeserializer* _deserializer             = nullptr;
    object_ptr_t _instance                   = nullptr;
    var_t _impl;
    size_t _index = -1;
  };

  ///////////////////////////////////////////

  using trackervect_t = std::unordered_map<std::string, object_ptr_t>;
  trackervect_t _reftracker;
};

} // namespace ork::reflect
