////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IArray.h"

#include <ork/config/config.h>

namespace ork {
class Object;
}

namespace ork { namespace reflect {

class IObjectArray : public IArray {
public:
  virtual object_ptr_t accessObject(object_ptr_t, size_t) const           = 0;
  virtual object_constptr_t accessObject(object_constptr_t, size_t) const = 0;
};

}} // namespace ork::reflect
