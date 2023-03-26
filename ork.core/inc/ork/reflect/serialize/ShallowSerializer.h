////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/serialize/LayerSerializer.h>

namespace ork { namespace reflect { namespace serdes {

class ShallowSerializer : public LayerSerializer {
public:
  ShallowSerializer(ISerializer& serializer);
};

inline ShallowSerializer::ShallowSerializer(ISerializer& serializer)
    : LayerSerializer(serializer) {
}

}}} // namespace ork::reflect::serialize
