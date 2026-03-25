////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ObjectProperty.h"
#include <ork/config/config.h>
#include <ork/kernel/varmap.inl>

namespace ork { namespace reflect {

using map_abstract_item_t = svar128_t;
using map_pair_t = std::pair<map_abstract_item_t,map_abstract_item_t>;
using map_kvarray_t = std::vector<map_pair_t>;

class IMap : public ObjectProperty {

public:

  virtual size_t elementCount(object_constptr_t obj) const = 0;
  static const int kDeserializeInsertElement = -1;
  virtual bool isMultiMap(object_constptr_t obj) const = 0;
  virtual map_kvarray_t enumerateElements(object_constptr_t obj) const = 0;

  // abstract interface

  virtual void insertDefaultElement(object_ptr_t obj,map_abstract_item_t key) const = 0;
  virtual void setElement(object_ptr_t obj,map_abstract_item_t key, map_abstract_item_t val) const = 0;
  virtual void removeElement(object_ptr_t obj,map_abstract_item_t key) const = 0;

  // For maps whose ValueType IS svar128_t (raw variant maps).
  // setElement() cannot be used in that case because sizeof(svar128_t) > ksize.
  virtual bool isRawVariantMap() const { return false; }
  virtual void setRawVariantElement(object_ptr_t obj, map_abstract_item_t key, const svar128_t& raw_val) const {}

protected:
  IMap() : ObjectProperty() {
  }
};

}} // namespace ork::reflect
