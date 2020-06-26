////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include "ObjectProperty.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class IArray : public ObjectProperty {
public:
  virtual void deserializeItem(IDeserializer&, Object*, size_t) const   = 0;
  virtual void serializeItem(ISerializer&, const Object*, size_t) const = 0;
  virtual size_t count(const Object*) const                             = 0;
  virtual void resize(Object* obj, size_t size) const                   = 0;

private:
  void deserialize(IDeserializer&, object_ptr_t) const override;
  void serialize(ISerializer&, object_constptr_t) const override;

protected:
  IArray() {
  }
};

}} // namespace ork::reflect
