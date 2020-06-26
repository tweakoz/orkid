////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IObjectMap.h"

namespace ork { namespace reflect {

template <typename KeyType> class AccessorObjectMap : public IObjectMap {
public:
  typedef void (*SerializationFunction)(BidirectionalSerializer&, const KeyType&, const Object*);

  AccessorObjectMap(
      const Object* (Object::*get)(const KeyType&, int) const,
      Object* (Object::*access)(const KeyType&, int),
      void (Object::*erase)(const KeyType&, int),
      void (Object::*mSerializer)(SerializationFunction, BidirectionalSerializer&) const);

private:
  Object* AccessItem(IDeserializer& key, int, Object*) const override;
  const Object* AccessItem(IDeserializer& key, int, const Object*) const override;

  void deserializeItem(IDeserializer* value, IDeserializer& key, int, Object*) const override;
  void serializeItem(ISerializer& value, IDeserializer& key, int, const Object*) const override;

  void deserialize(IDeserializer& serializer, Object* obj) const override;
  void serialize(ISerializer& serializer, const Object* obj) const override;

  static void _doSerialize(BidirectionalSerializer& bidi, const KeyType& key, const Object* value);

  const Object* (Object::*mGetter)(const KeyType&, int) const;
  Object* (Object::*mAccessor)(const KeyType&, int);
  void (Object::*mEraser)(const KeyType&, int);
  void (Object::*mSerializer)(SerializationFunction, BidirectionalSerializer&) const;
};

}} // namespace ork::reflect
