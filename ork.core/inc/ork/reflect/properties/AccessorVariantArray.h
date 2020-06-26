////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IArray.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class AccessorVariantArray : public IArray {
public:
  AccessorVariantArray(
      bool (Object::*serialize_item)(ISerializer&, size_t) const,
      bool (Object::*deserialize_item)(IDeserializer&, size_t),
      size_t (Object::*count)() const,
      bool (Object::*resize)(size_t));

private:
  void serializeItem(ISerializer&, const Object*, size_t) const override;
  void deserializeItem(IDeserializer&, Object*, size_t) const override;
  size_t count(const Object*) const override;
  void resize(Object*, size_t) const override;

  bool (Object::*mSerializeItem)(ISerializer&, size_t) const;
  bool (Object::*mDeserializeItem)(IDeserializer&, size_t);
  size_t (Object::*mCount)() const;
  bool (Object::*mResize)(size_t);
};

}} // namespace ork::reflect
