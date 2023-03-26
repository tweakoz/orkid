////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
