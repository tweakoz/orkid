////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class IMap : public ObjectProperty {

public:
  virtual size_t itemCount(const Object* obj) const = 0;

  static const int kDeserializeInsertItem = -1;

  virtual void deserializeItem(IDeserializer* value, IDeserializer& key, int, Object*) const   = 0;
  virtual void serializeItem(ISerializer& value, IDeserializer& key, int, const Object*) const = 0;
  virtual bool isMultiMap(const Object* obj) const                                             = 0;

protected:
  IMap() {
  }
};

}} // namespace ork::reflect
