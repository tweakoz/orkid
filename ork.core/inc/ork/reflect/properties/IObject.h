////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"
#include <ork/reflect/Serializable.h>
#include <ork/rtti/RTTI.h>

#include <ork/config/config.h>

namespace ork {
class Object;
}

namespace ork { namespace reflect {

class IObject : public ObjectProperty {
  DECLARE_TRANSPARENT_CASTABLE(IObject, ObjectProperty)
public:
  virtual Object* Access(Object*) const             = 0;
  virtual const Object* Access(const Object*) const = 0;
};

}} // namespace ork::reflect
