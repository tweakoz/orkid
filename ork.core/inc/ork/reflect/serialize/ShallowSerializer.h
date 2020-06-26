////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/serialize/LayerSerializer.h>

namespace ork { namespace reflect { namespace serialize {

class ShallowSerializer : public LayerSerializer {
public:
  ShallowSerializer(ISerializer& serializer);

  void serializeSharedObject(object_constptr_t object_inp);
};

inline ShallowSerializer::ShallowSerializer(ISerializer& serializer)
    : LayerSerializer(serializer) {
}

inline void ShallowSerializer::serializeSharedObject(object_constptr_t object_inp) {
  // TODO replace long with variant
  // long deserialized;
  // bool LayerDeserializer::deserialize(deserialized);
  // object = reinterpret_cast<rtti::ICastable*>(deserialized);
  // return result;
  // return LayerSerializer::serializeSharedObject(reinterpret_cast<long>(object));
}

}}} // namespace ork::reflect::serialize
