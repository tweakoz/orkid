////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"
#include <ork/reflect/Serializable.h>
#include <ork/rtti/RTTI.h>

#include <ork/config/config.h>

namespace ork {
struct Object;
}

namespace ork { namespace reflect {

class IObject : public ObjectProperty {
public:
  virtual object_ptr_t access(object_ptr_t) const           = 0;
  virtual object_constptr_t access(object_constptr_t) const = 0;
};

}} // namespace ork::reflect
