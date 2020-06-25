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
  /*virtual*/ Object* AccessObject(Object*, size_t) const;
  /*virtual*/ const Object* AccessObject(const Object*, size_t) const;
  /*virtual*/ size_t Count(const Object*) const;
  /*virtual*/ bool Resize(Object* obj, size_t size) const;

  /*virtual*/ bool DeserializeItem(IDeserializer&, Object*, size_t) const;
  /*virtual*/ bool SerializeItem(ISerializer&, const Object*, size_t) const;
  /*virtual*/ bool Deserialize(IDeserializer&, Object*) const;
  /*virtual*/ bool Serialize(ISerializer&, const Object*) const;

  Object* (Object::*mAccessor)(size_t);
  size_t (Object::*mCounter)() const;
  void (Object::*mResizer)(size_t);
};

}} // namespace ork::reflect
