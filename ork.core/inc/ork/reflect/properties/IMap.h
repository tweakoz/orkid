////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class IMap : public ObjectProperty {

public:
  virtual size_t elementCount(object_constptr_t obj) const = 0;

  static const int kDeserializeInsertElement = -1;

  virtual bool isMultiMap(object_constptr_t obj) const = 0;

protected:
  IMap() {
  }
};

}} // namespace ork::reflect
