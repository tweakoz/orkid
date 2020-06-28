////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
  typedef typename MapType::key_type KeyType;
  typedef typename MapType::mapped_type ValueType;

  using ItemSerializeFunction   = typename ITypedMap<KeyType, ValueType>::ItemSerializeFunction;
  using ItemDeserializeFunction = typename ITypedMap<KeyType, ValueType>::ItemDeserializeFunction;

  DirectTypedMap(MapType Object::*);

  MapType& GetMap(object_ptr_t obj) const;
  const MapType& GetMap(object_constptr_t obj) const;

  bool isMultiMap(object_constptr_t obj) const override;

protected:
  bool ReadItem(object_constptr_t, const KeyType&, int, ValueType&) const override;
  bool WriteItem(object_ptr_t, const KeyType&, int, const ValueType*) const override;
  bool EraseItem(object_ptr_t, const KeyType&, int) const override;
  // bool MapSerialization(ItemSerializeFunction, BidirectionalSerializer&, object_constptr_t) const override;
  void MapSerialization(ItemSerializeFunction, ISerializer&, object_constptr_t) const override;
  void MapDeserialization(ItemDeserializeFunction, IDeserializer&, object_ptr_t) const override;

  size_t itemCount(object_constptr_t obj) const override {
    return int(GetMap(obj).size());
  }
  bool GetKey(object_constptr_t, int idx, KeyType&) const override;
  bool GetVal(object_constptr_t, const KeyType& k, ValueType& v) const override;

private:
  MapType Object::*mProperty;
};

} // namespace ork::reflect
