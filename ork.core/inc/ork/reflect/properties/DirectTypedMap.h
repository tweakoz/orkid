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

  using ElementSerializeFunction   = typename ITypedMap<KeyType, ValueType>::ElementSerializeFunction;
  using ElementDeserializeFunction = typename ITypedMap<KeyType, ValueType>::ElementDeserializeFunction;

  DirectTypedMap(MapType Object::*);

  MapType& GetMap(object_ptr_t obj) const;
  const MapType& GetMap(object_constptr_t obj) const;

  bool isMultiMap(object_constptr_t obj) const override;

protected:
  bool ReadElement(object_constptr_t, const KeyType&, int, ValueType&) const override;
  bool WriteElement(object_ptr_t, const KeyType&, int, const ValueType*) const override;
  bool EraseElement(object_ptr_t, const KeyType&, int) const override;
  // bool MapSerialization(ElementSerializeFunction, BidirectionalSerializer&, object_constptr_t) const override;
  void MapSerialization(ElementSerializeFunction, ISerializer&, object_constptr_t) const override;
  void MapDeserialization(ElementDeserializeFunction, IDeserializer::node_ptr_t) const override;

  size_t elementCount(object_constptr_t obj) const override {
    return int(GetMap(obj).size());
  }
  bool GetKey(object_constptr_t, int idx, KeyType&) const override;
  bool GetVal(object_constptr_t, const KeyType& k, ValueType& v) const override;

private:
  MapType Object::*mProperty;
};

} // namespace ork::reflect
