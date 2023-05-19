////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "IObjectArray.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class AccessorObjectArray : public IObjectArray {
public:
  AccessorObjectArray(
      object_ptr_t (Object::*accessor)(size_t),
      size_t (Object::*counter)() const,
      void (Object::*resizer)(size_t) = 0);

private:
  object_ptr_t accessObject(object_ptr_t, size_t) const override;
  object_constptr_t accessObject(object_constptr_t, size_t) const override;
  size_t count(object_constptr_t) const override;
  void resize(object_ptr_t obj, size_t size) const override;

  void deserializeElement(serdes::node_ptr_t) const override;
  void serializeElement(serdes::node_ptr_t) const override;
  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;
  array_varray_t enumerateElements(object_constptr_t obj) const override;

  object_ptr_t (Object::*_accessor)(size_t);
  size_t (Object::*_counter)() const;
  void (Object::*_resizer)(size_t);
};

}} // namespace ork::reflect
