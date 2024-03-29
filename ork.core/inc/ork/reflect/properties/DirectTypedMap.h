////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedMap.h"

#include <ork/config/config.h>

namespace ork::reflect {

template <typename MapType>
class DirectTypedMap //
    : public ITypedMap<
          typename MapType::key_type, //
          typename MapType::mapped_type> {
public:

  using maptype_t = MapType;
  using KeyType = typename MapType::key_type;
  using ValueType = typename MapType::mapped_type;

  DirectTypedMap(MapType Object::*);

  MapType& GetMap(object_ptr_t obj) const;
  const MapType& GetMap(object_constptr_t obj) const;

  bool isMultiMap(object_constptr_t obj) const final;

protected:
  bool ReadElement(object_constptr_t, const KeyType&, int, ValueType&) const final;
  bool WriteElement(object_ptr_t, const KeyType&, int, const ValueType*) const final;
  bool EraseElement(object_ptr_t, const KeyType&, int) const final;

  size_t elementCount(object_constptr_t obj) const final {
    return int(GetMap(obj).size());
  }
  bool GetKey(object_constptr_t, int idx, KeyType&) const final;
  bool GetVal(object_constptr_t, const KeyType& k, ValueType& v) const final;

  // abstract interface
  
  void insertDefaultElement(object_ptr_t obj,map_abstract_item_t key) const final;
  void setElement(object_ptr_t obj,map_abstract_item_t key, map_abstract_item_t val) const final;
  void removeElement(object_ptr_t obj, map_abstract_item_t key) const final;

private:
  MapType Object::*mProperty;
};

} // namespace ork::reflect
