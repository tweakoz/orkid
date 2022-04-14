////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
