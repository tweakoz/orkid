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

template <typename MapType> //
class DirectObjectMap : public ITypedMap<typename MapType::key_type, object_ptr_t> {
public:
  using KeyType               = typename MapType::key_type;
  using ItemSerializeFunction = typename ITypedMap<KeyType, object_ptr_t>::ItemSerializeFunction;

  DirectObjectMap(MapType Object::*);

  MapType& GetMap(object_ptr_t obj) const;
  const MapType& GetMap(object_constptr_t obj) const;

  bool isMultiMap(object_constptr_t obj) const override;

protected:
  bool ReadItem(object_constptr_t, const KeyType&, int, object_ptr_t&) const override;
  bool WriteItem(object_ptr_t, const KeyType&, int, const object_ptr_t*) const override;
  bool EraseItem(object_ptr_t, const KeyType&, int) const override;
  bool MapSerialization(ItemSerializeFunction, BidirectionalSerializer&, object_constptr_t) const override;

  size_t itemCount(object_constptr_t obj) const override {
    return GetMap(obj).size();
  }
  bool GetKey(object_constptr_t, int idx, KeyType&) const override;
  bool GetVal(object_constptr_t, const KeyType& k, object_ptr_t& v) const override;

private:
  MapType Object::*_member;
};

}} // namespace ork::reflect
