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
};

inline ShallowSerializer::ShallowSerializer(ISerializer& serializer)
    : LayerSerializer(serializer) {
}

}}} // namespace ork::reflect::serialize
