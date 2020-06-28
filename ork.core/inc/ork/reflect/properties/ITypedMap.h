////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IMap.h"
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename KeyType, typename ValueType> //
class ITypedMap : public IMap {

public:
  // using ItemBiSerializeFunction = void (*)(BidirectionalSerializer&, KeyType&, ValueType&);
  using ItemSerializeFunction   = void (*)(ISerializer&, const KeyType&, const ValueType&);
  using ItemDeserializeFunction = void (*)(IDeserializer&, KeyType&, ValueType&);

protected:
  virtual bool GetKey(object_constptr_t, int idx, KeyType&) const                   = 0;
  virtual bool GetVal(object_constptr_t, const KeyType& k, ValueType& v) const      = 0;
  virtual bool ReadItem(object_constptr_t, const KeyType&, int, ValueType&) const   = 0;
  virtual bool EraseItem(object_ptr_t, const KeyType&, int) const                   = 0;
  virtual bool WriteItem(object_ptr_t, const KeyType&, int, const ValueType*) const = 0;
  // virtual bool MapBiSerialization(ItemBiSerializeFunction, BidirectionalSerializer&, object_constptr_t) const = 0;
  virtual void MapSerialization(ItemSerializeFunction, ISerializer&, object_constptr_t) const  = 0;
  virtual void MapDeserialization(ItemDeserializeFunction, IDeserializer&, object_ptr_t) const = 0;

  ITypedMap()
      : IMap() {
  }

private:
  static void _doDeserialize(IDeserializer&, KeyType&, ValueType&);
  static void _doSerialize(ISerializer&, const KeyType&, const ValueType&);

  void deserialize(IDeserializer&, object_ptr_t) const override;
  void serialize(ISerializer&, object_constptr_t) const override;
};

}} // namespace ork::reflect
