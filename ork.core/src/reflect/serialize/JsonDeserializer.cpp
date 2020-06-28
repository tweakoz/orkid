////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

#include <ork/orkprotos.h>
#include <cstring>

using namespace rapidjson;

namespace ork::reflect::serialize {
//////////////////////////////////////////////////////////////////////////////

static int unhex(char c);

JsonDeserializer::JsonDeserializer(stream::IInputStream& stream)
    : mStream(stream) {
}

void JsonDeserializer::deserializeItem() {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeObjectProperty(
    const ObjectProperty* prop, //
    object_ptr_t object) {
  prop->deserialize(*this, object);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeSharedObject(object_ptr_t& instance_out) {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::beginCommand(Command& command) {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::endCommand(const Command& command) {
}

//////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
