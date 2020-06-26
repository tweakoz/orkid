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
  using ItemSerializeFunction = void (*)(BidirectionalSerializer&, KeyType&, ValueType&);

protected:
  virtual bool GetKey(const Object*, int idx, KeyType&) const                                         = 0;
  virtual bool GetVal(const Object*, const KeyType& k, ValueType& v) const                            = 0;
  virtual bool ReadItem(const Object*, const KeyType&, int, ValueType&) const                         = 0;
  virtual bool EraseItem(Object*, const KeyType&, int) const                                          = 0;
  virtual bool WriteItem(Object*, const KeyType&, int, const ValueType*) const                        = 0;
  virtual bool MapSerialization(ItemSerializeFunction, BidirectionalSerializer&, const Object*) const = 0;

  ITypedMap()
      : IMap() {
  }

private:
  static void _doDeserialize(BidirectionalSerializer&, KeyType&, ValueType&);
  static void _doSerialize(BidirectionalSerializer&, KeyType&, ValueType&);
  void deserialize(IDeserializer&, Object*) const override;
  void serialize(ISerializer&, const Object*) const override;
  void deserializeItem(IDeserializer* value, IDeserializer& key, int, Object*) const override;
  void serializeItem(ISerializer& value, IDeserializer& key, int, const Object*) const override;
};

}} // namespace ork::reflect
