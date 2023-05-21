////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ITyped.h"

#include <ork/config/config.h>

namespace ork::reflect {

class DirectEnumBase : public ObjectProperty {
};

template <typename T> class DirectEnum : public DirectEnumBase {

public:

  T Object::*_member;

  DirectEnum(T Object::*);

  void get(T&, object_constptr_t) const;
  void set(const T&, object_ptr_t) const;
  void deserialize(serdes::node_ptr_t) const final;
  void serialize(serdes::node_ptr_t) const final;
};

} // namespace ork::reflect
