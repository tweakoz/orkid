////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/serialize/NullSerializer.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/rtti/Category.h>

namespace ork { namespace reflect { namespace serialize {

void NullSerializer::serialize(const bool&) {
}

void NullSerializer::serialize(const char&) {
}

void NullSerializer::serialize(const short&) {
}

void NullSerializer::serialize(const int&) {
}

void NullSerializer::serialize(const long&) {
}

void NullSerializer::serialize(const float&) {
}

void NullSerializer::serialize(const double&) {
}

void NullSerializer::serializeSharedObject(object_constptr_t) {
}

void NullSerializer::serialize(const PieceString&) {
}

void NullSerializer::Hint(const PieceString&) {
}
void NullSerializer::Hint(const PieceString&, intptr_t ival) {
}

void NullSerializer::serializeData(const uint8_t*, size_t) {
}

void NullSerializer::serializeObjectProperty(
    const ObjectProperty* prop, //
    object_constptr_t instance) {
  return prop->serialize(*this, instance);
}

void NullSerializer::beginCommand(const Command&) {
}

void NullSerializer::endCommand(const Command&) {
}

}}} // namespace ork::reflect::serialize
