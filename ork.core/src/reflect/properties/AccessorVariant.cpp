////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/AccessorVariant.h>
#include <ork/object/Object.h>

namespace ork { namespace reflect {

AccessorVariant::AccessorVariant(
    bool (Object::*ser)(serdes::ISerializer&) const, //
    bool (Object::*deser)(serdes::IDeserializer&))
    : _serialize(ser)
    , _deserialize(deser) {
}

void AccessorVariant::deserialize(serdes::node_ptr_t desernode) const {
  //(object.get()->*mDeserialize)(deserializer);
}

void AccessorVariant::serialize(serdes::node_ptr_t desernode) const {
  //(object.get()->*mSerialize)(serializer);
}

}} // namespace ork::reflect
