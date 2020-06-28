////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/AccessorVariantArray.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

AccessorVariantArray::AccessorVariantArray(
    void (Object::*serialize_item)(ISerializer&, size_t) const,
    void (Object::*deserialize_element)(IDeserializer::node_ptr_t),
    size_t (Object::*count)() const,
    void (Object::*resize)(size_t))
    : mSerializeItem(serialize_item)
    , _deserializeElement(deserialize_element)
    , mCount(count)
    , mResize(resize) {
}

void AccessorVariantArray::deserializeElement(IDeserializer::node_ptr_t desernode) const {
  // return (instance.get()->*_deserializeElement)(deserializer, index);
}

void AccessorVariantArray::serializeItem(
    ISerializer& serializer, //
    object_constptr_t instance,
    size_t index) const {
  return (instance.get()->*mSerializeItem)(serializer, index);
}

size_t AccessorVariantArray::count(object_constptr_t instance) const {
  return (instance.get()->*mCount)();
}

void AccessorVariantArray::resize(object_ptr_t instance, size_t size) const {
  return (instance.get()->*mResize)(size);
}

}} // namespace ork::reflect
