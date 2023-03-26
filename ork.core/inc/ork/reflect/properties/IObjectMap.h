////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
