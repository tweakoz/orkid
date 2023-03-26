////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/AccessorVariantArray.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

AccessorVariantArray::AccessorVariantArray(
    void (Object::*serialize_item)(ISerializer&, size_t) const,
    void (Object::*deserialize_element)(serdes::node_ptr_t),
    size_t (Object::*count)() const,
    void (Object::*resize)(size_t))
    : mSerializeItem(serialize_item)
    , _deserializeElement(deserialize_element)
    , mCount(count)
    , mResize(resize) {
}

void AccessorVariantArray::deserializeElement(serdes::node_ptr_t desernode) const {
  // return (instance.get()->*_deserializeElement)(deserializer, index);
}

void AccessorVariantArray::serializeElement(serdes::node_ptr_t sernode) const {
  // return (instance.get()->*mSerializeItem)(serializer, index);
}

size_t AccessorVariantArray::count(object_constptr_t instance) const {
  return (instance.get()->*mCount)();
}

void AccessorVariantArray::resize(object_ptr_t instance, size_t size) const {
  return (instance.get()->*mResize)(size);
}

}} // namespace ork::reflect
