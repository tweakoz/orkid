////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IObjectMap.h"

namespace ork { namespace reflect {

template <typename KeyType> //
class AccessorObjectMap : public IObjectMap {
public:
  typedef void (*SerializationFunction)(serdes::BidirectionalSerializer&, const KeyType&, object_constptr_t);

  AccessorObjectMap(
      object_constptr_t (Object::*get)(const KeyType&, int) const,
      object_ptr_t (Object::*access)(const KeyType&, int),
      void (Object::*erase)(const KeyType&, int),
      void (Object::*mSerializer)(SerializationFunction, serdes::BidirectionalSerializer&) const);

private:
  object_ptr_t accessItem(serdes::IDeserializer& key, int, object_ptr_t instance) const override;
  object_constptr_t accessItem(serdes::IDeserializer& key, int, object_constptr_t instance) const override;

  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;

  static void _serdesimpl(
      serdes::BidirectionalSerializer& bidi, //
      const KeyType& key,
      object_constptr_t value);

  object_constptr_t (Object::*_getter)(const KeyType&, int) const;
  object_ptr_t (Object::*_accessor)(const KeyType&, int);
  void (Object::*_eraser)(const KeyType&, int);
  void (Object::*_serializer)(SerializationFunction, serdes::BidirectionalSerializer&) const;
};

}} // namespace ork::reflect
