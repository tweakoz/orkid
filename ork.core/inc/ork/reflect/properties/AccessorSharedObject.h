////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/ISharedObject.h>

#include <ork/config/config.h>

namespace ork {

class Object;

namespace reflect {

class AccessorSharedObject : public ISharedObject {
public:
  AccessorSharedObject(object_ptr_t (Object::*)());

  bool Serialize(ISerializer&, const Object*) const override;
  bool Deserialize(IDeserializer&, Object*) const override;
  object_ptr_t Access(Object*) const override;
  object_constptr_t Access(const Object*) const override;

private:
  object_ptr_t (Object::*mObjectAccessor)();
};

} // namespace reflect
} // namespace ork
