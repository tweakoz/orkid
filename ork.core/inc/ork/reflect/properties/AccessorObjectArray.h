////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IObjectArray.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class AccessorObjectArray : public IObjectArray {
public:
  AccessorObjectArray(Object* (Object::*accessor)(size_t), size_t (Object::*counter)() const, void (Object::*resizer)(size_t) = 0);

private:
  Object* AccessObject(Object*, size_t) const override;
  const Object* AccessObject(const Object*, size_t) const override;
  size_t count(const Object*) const override;
  void resize(Object* obj, size_t size) const override;

  void deserializeItem(IDeserializer&, Object*, size_t) const override;
  void serializeItem(ISerializer&, const Object*, size_t) const override;
  void deserialize(IDeserializer&, object_ptr_t) const override;
  void serialize(ISerializer&, object_constptr_t) const override;

  Object* (Object::*mAccessor)(size_t);
  size_t (Object::*mCounter)() const;
  void (Object::*mResizer)(size_t);
};

}} // namespace ork::reflect
