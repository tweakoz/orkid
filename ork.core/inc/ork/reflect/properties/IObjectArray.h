////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "IArray.h"

#include <ork/config/config.h>

namespace ork {
struct Object;
}

namespace ork { namespace reflect {

class IObjectArray : public IArray {
public:
  virtual object_ptr_t accessObject(object_ptr_t, size_t) const           = 0;
  virtual object_constptr_t accessObject(object_constptr_t, size_t) const = 0;
};

}} // namespace ork::reflect
