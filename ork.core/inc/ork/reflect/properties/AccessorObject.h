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
  AccessorObject(Object* (Object::*)());

  void serialize(ISerializer&, const Object*) const override;
  void deserialize(IDeserializer&, Object*) const override;
  Object* Access(Object*) const override;
  const Object* Access(const Object*) const override;

private:
  Object* (Object::*mObjectAccessor)();
};

} // namespace reflect
} // namespace ork
