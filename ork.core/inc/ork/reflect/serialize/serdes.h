////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/MutableString.h>
#include <ork/kernel/svariant.h>
#include <stdint.h>
#include <unordered_set>
#include <unordered_map>
#include <boost/uuid/uuid.hpp>

#include "../types.h"

namespace ork::reflect::serdes {

enum struct NodeType {
  UNKNOWN, //
  LEAF,
  OBJECT,
  PROPERTIES,
  MAP,
  MAP_ELEMENT_LEAF,
  MAP_ELEMENT_OBJECT,
  ARRAY,
  ARRAY_ELEMENT_LEAF,
  ARRAY_ELEMENT_OBJECT,
};

struct Node {
  node_ptr_t _parent                       = nullptr;
  const reflect::ObjectProperty* _property = nullptr;
  ISerializer* _serializer                 = nullptr;
  IDeserializer* _deserializer             = nullptr;
  object_constptr_t _ser_instance          = nullptr;
  object_ptr_t _deser_instance             = nullptr;
  var_t _impl;
  std::string _key;
  std::string _name;
  var_t _value;
  size_t _index       = 0;
  size_t _multiindex  = 0;
  size_t _numchildren = 0;
  NodeType _type      = NodeType::UNKNOWN;
};

} // namespace ork::reflect::serdes
