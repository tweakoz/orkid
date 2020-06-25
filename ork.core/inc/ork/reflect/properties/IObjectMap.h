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
  DECLARE_TRANSPARENT_CASTABLE(IObjectMap, IMap)
public:
  virtual Object* AccessItem(IDeserializer& key, int, Object*) const             = 0;
  virtual const Object* AccessItem(IDeserializer& key, int, const Object*) const = 0;
};

}} // namespace ork::reflect
