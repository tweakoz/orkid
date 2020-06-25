////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/ITypedMap.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename MapType> //
class DirectSharedObjectMap : public ITypedMap<typename MapType::key_type, object_ptr_t> {
public:
  using KeyType               = typename MapType::key_type;
  using ItemSerializeFunction = typename ITypedMap<KeyType, object_ptr_t>::ItemSerializeFunction;

  DirectSharedObjectMap(MapType Object::*);

  MapType& GetMap(Object* obj) const;
  const MapType& GetMap(const Object* obj) const;

  bool IsMultiMap(const Object* obj) const override;

protected:
  bool ReadItem(const Object*, const KeyType&, int, object_ptr_t&) const override;
  bool WriteItem(Object*, const KeyType&, int, const object_ptr_t*) const override;
  bool EraseItem(Object*, const KeyType&, int) const override;
  bool MapSerialization(ItemSerializeFunction, BidirectionalSerializer&, const Object*) const override;

  int GetSize(const Object* obj) const override {
    return int(GetMap(obj).size());
  }
  bool GetKey(const Object*, int idx, KeyType&) const override;
  bool GetVal(const Object*, const KeyType& k, object_ptr_t& v) const override;

private:
  MapType Object::*mProperty;
};

}} // namespace ork::reflect
