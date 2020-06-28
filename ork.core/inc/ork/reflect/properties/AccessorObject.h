////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IObject.h"

#include <ork/config/config.h>

namespace ork {

class Object;

namespace reflect {

class AccessorObject : public IObject {
public:
  AccessorObject(object_ptr_t (Object::*)());

  void serialize(ISerializer&, object_constptr_t) const override;
  void deserialize(IDeserializer::node_ptr_t) const override;
  object_ptr_t access(object_ptr_t) const override;
  object_constptr_t access(object_constptr_t) const override;

private:
  object_ptr_t (Object::*_accessor)();
};

} // namespace reflect
} // namespace ork
