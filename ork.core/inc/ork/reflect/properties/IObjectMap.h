////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>

#include "IMap.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class IObjectMap : public IMap {
public:
  virtual object_ptr_t accessItem(serdes::IDeserializer& key, int, object_ptr_t) const           = 0;
  virtual object_constptr_t accessItem(serdes::IDeserializer& key, int, object_constptr_t) const = 0;
};

}} // namespace ork::reflect
