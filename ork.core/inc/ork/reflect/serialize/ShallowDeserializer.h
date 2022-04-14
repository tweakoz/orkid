////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/serialize/LayerDeserializer.h>

namespace ork { namespace reflect { namespace serdes {

class ShallowDeserializer : public LayerDeserializer {
public:
  ShallowDeserializer(IDeserializer& deserializer);

  void deserialize(object_ptr_t& object_out);
};

inline ShallowDeserializer::ShallowDeserializer(IDeserializer& deserializer)
    : LayerDeserializer(deserializer) {
}

inline void ShallowDeserializer::deserialize(object_ptr_t& object_out) {
  OrkAssert(false);
  // TODO replace long with variant
  // long deserialized;
  // bool LayerDeserializer::deserialize(deserialized);
  // object = reinterpret_cast<rtti::ICastable*>(deserialized);
  // return result;
}

}}} // namespace ork::reflect::serialize
