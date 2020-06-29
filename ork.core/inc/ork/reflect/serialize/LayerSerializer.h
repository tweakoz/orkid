////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/Command.h>
#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serdes {

struct LayerSerializer : public ISerializer {

  LayerSerializer(ISerializer& serializer);

  node_ptr_t layerSerializeRoot(object_constptr_t);
  node_ptr_t serializeElement(node_ptr_t elemnode) override;

protected:
  ISerializer& _subserializer;
};

inline LayerSerializer::LayerSerializer(ISerializer& serializer)
    : _subserializer(serializer) {
}

inline ISerializer::node_ptr_t LayerSerializer::layerSerializeRoot(object_constptr_t instance) {
  return _subserializer.serializeRoot(instance);
}
inline ISerializer::node_ptr_t LayerSerializer::serializeElement(node_ptr_t elemnode) {
  return _subserializer.serializeElement(elemnode);
}

}}} // namespace ork::reflect::serialize
