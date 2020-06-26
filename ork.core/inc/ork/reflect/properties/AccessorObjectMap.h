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
  typedef void (*SerializationFunction)(BidirectionalSerializer&, const KeyType&, object_constptr_t);

  AccessorObjectMap(
      object_constptr_t (Object::*get)(const KeyType&, int) const,
      object_ptr_t (Object::*access)(const KeyType&, int),
      void (Object::*erase)(const KeyType&, int),
      void (Object::*mSerializer)(SerializationFunction, BidirectionalSerializer&) const);

private:
  object_ptr_t AccessItem(IDeserializer& key, int, object_ptr_t) const override;
  object_constptr_t AccessItem(IDeserializer& key, int, object_constptr_t) const override;

  void deserializeItem(IDeserializer* value, IDeserializer& key, int, object_ptr_t) const override;
  void serializeItem(ISerializer& value, IDeserializer& key, int, object_constptr_t) const override;

  void deserialize(IDeserializer& serializer, object_ptr_t) const override;
  void serialize(ISerializer& serializer, object_constptr_t) const override;

  static void _doSerialize(BidirectionalSerializer& bidi, const KeyType& key, object_constptr_t value);

  object_constptr_t (Object::*mGetter)(const KeyType&, int) const;
  object_ptr_t (Object::*mAccessor)(const KeyType&, int);
  void (Object::*mEraser)(const KeyType&, int);
  void (Object::*mSerializer)(SerializationFunction, BidirectionalSerializer&) const;
};

}} // namespace ork::reflect
