////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "IArray.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class AccessorVariantArray : public IArray {
public:
  AccessorVariantArray(
      void (Object::*serialize_item)(serdes::ISerializer&, size_t) const,
      void (Object::*deserialize_element)(serdes::node_ptr_t),
      size_t (Object::*count)() const,
      void (Object::*resize)(size_t));

private:
  void serializeElement(serdes::node_ptr_t) const override;
  void deserializeElement(serdes::node_ptr_t) const override;
  size_t count(object_constptr_t) const override;
  void resize(object_ptr_t, size_t) const override;

  void (Object::*mSerializeItem)(serdes::ISerializer&, size_t) const;
  void (Object::*_deserializeElement)(serdes::node_ptr_t);
  size_t (Object::*mCount)() const;
  void (Object::*mResize)(size_t);

  array_varray_t enumerateElements(object_constptr_t obj) const override;

};

}} // namespace ork::reflect
