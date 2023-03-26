////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "IObject.h"

#include <ork/config/config.h>

namespace ork {

struct Object;

namespace reflect {

class AccessorObject : public IObject {
public:
  AccessorObject(object_ptr_t (Object::*)());

  void serialize(serdes::node_ptr_t) const override;
  void deserialize(serdes::node_ptr_t) const override;
  object_ptr_t access(object_ptr_t) const override;
  object_constptr_t access(object_constptr_t) const override;

private:
  object_ptr_t (Object::*_accessor)();
};

} // namespace reflect
} // namespace ork
