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

using map_abstract_item_t = svar256_t;
using map_pair_t = std::pair<map_abstract_item_t,map_abstract_item_t>;
using map_kvarray_t = std::vector<map_pair_t>;

class IMap : public ObjectProperty {

public:

  virtual size_t elementCount(object_constptr_t obj) const = 0;
  static const int kDeserializeInsertElement = -1;
  virtual bool isMultiMap(object_constptr_t obj) const = 0;
  virtual map_kvarray_t enumerateElements(object_constptr_t obj) const = 0;
  virtual void insertDefaultElement(object_ptr_t obj,map_abstract_item_t key) const = 0;
protected:
  IMap() : ObjectProperty() {
  }
};

}} // namespace ork::reflect
