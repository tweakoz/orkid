////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedMap.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename MapType> class DirectTypedMap : public ITypedMap<typename MapType::key_type, typename MapType::mapped_type> {
public:
  typedef typename MapType::key_type KeyType;
  typedef typename MapType::mapped_type ValueType;
  typedef typename ITypedMap<KeyType, ValueType>::ItemSerializeFunction ItemSerializeFunction;

  DirectTypedMap(MapType Object::*);

  MapType& GetMap(Object* obj) const;
  const MapType& GetMap(const Object* obj) const;

  bool IsMultiMap(const Object* obj) const override;

protected:
  bool ReadItem(const Object*, const KeyType&, int, ValueType&) const override;
  bool WriteItem(Object*, const KeyType&, int, const ValueType*) const override;
  bool EraseItem(Object*, const KeyType&, int) const override;
  bool MapSerialization(ItemSerializeFunction, BidirectionalSerializer&, const Object*) const override;

  int GetSize(const Object* obj) const override {
    return int(GetMap(obj).size());
  }
  bool GetKey(const Object*, int idx, KeyType&) const override;
  bool GetVal(const Object*, const KeyType& k, ValueType& v) const override;

private:
  MapType Object::*mProperty;
};

}} // namespace ork::reflect
