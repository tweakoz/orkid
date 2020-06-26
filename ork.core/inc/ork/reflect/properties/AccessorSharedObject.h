////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ISharedObject.h"

#include <ork/config/config.h>

namespace ork {

class Object;

namespace reflect {

class AccessorSharedObject : public ISharedObject {
public:
  AccessorSharedObject(object_ptr_t (Object::*)());

  void serialize(ISerializer&, object_constptr_t) const override;
  void deserialize(IDeserializer&, object_ptr_t) const override;
  object_ptr_t Access(Object*) const override;
  object_constptr_t Access(const Object*) const override;

private:
  object_ptr_t (Object::*mObjectAccessor)();
};

} // namespace reflect
} // namespace ork
