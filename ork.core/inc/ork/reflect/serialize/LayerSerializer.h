////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
  node_ptr_t serializeContainerElement(node_ptr_t elemnode) override;

protected:
  ISerializer& _subserializer;
};

inline LayerSerializer::LayerSerializer(ISerializer& serializer)
    : _subserializer(serializer) {
}

inline serdes::node_ptr_t LayerSerializer::layerSerializeRoot(object_constptr_t instance) {
  return _subserializer.serializeRoot(instance);
}
inline serdes::node_ptr_t LayerSerializer::serializeContainerElement(node_ptr_t elemnode) {
  return _subserializer.serializeContainerElement(elemnode);
}

}}} // namespace ork::reflect::serdes
