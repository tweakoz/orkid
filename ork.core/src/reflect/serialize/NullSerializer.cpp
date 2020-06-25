////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/serialize/NullSerializer.h>

#include <ork/reflect/IProperty.h>
#include <ork/reflect/properties/I.h>
#include <ork/rtti/Category.h>

namespace ork { namespace reflect { namespace serialize {

bool NullSerializer::Serialize(const bool&) {
  return true;
}

bool NullSerializer::Serialize(const char&) {
  return true;
}

bool NullSerializer::Serialize(const short&) {
  return true;
}

bool NullSerializer::Serialize(const int&) {
  return true;
}

bool NullSerializer::Serialize(const long&) {
  return true;
}

bool NullSerializer::Serialize(const float&) {
  return true;
}

bool NullSerializer::Serialize(const double&) {
  return true;
}

bool NullSerializer::serializeObject(const rtti::ICastable*) {
  return true;
}

bool NullSerializer::Serialize(const PieceString&) {
  return true;
}

void NullSerializer::Hint(const PieceString&) {
}
void NullSerializer::Hint(const PieceString&, intptr_t ival) {
}

bool NullSerializer::SerializeData(unsigned char*, size_t) {
  return true;
}

bool NullSerializer::Serialize(const IProperty* prop) {
  return prop->Serialize(*this);
}

bool NullSerializer::serializeObjectProperty(
    const ObjectProperty* prop, //
    const Object* object) {
  return prop->Serialize(*this, object);
}

// bool NullSerializer::serializeObjectWithCategory(const rtti::Category* cat, const rtti::ICastable* object) {
// return cat->serializeObject(*this, object);
//}

bool NullSerializer::ReferenceObject(const rtti::ICastable*) {
  return true;
}

bool NullSerializer::BeginCommand(const Command&) {
  return true;
}

bool NullSerializer::EndCommand(const Command&) {
  return true;
}

}}} // namespace ork::reflect::serialize
