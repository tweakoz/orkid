////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/stream/IOutputStream.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/string/string.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>
#include <rapidjson/writer.h>
#include <rapidjson/document.h>

namespace ork::reflect::serialize {
////////////////////////////////////////////////////////////////////////////////
JsonSerializer::JsonSerializer(stream::IOutputStream& stream)
    : mStream(stream) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::beginCommand(const Command& command) {
  switch (command.Type()) {
    case Command::EOBJECT:
      break;
    case Command::EATTRIBUTE:
      break;
    case Command::EPROPERTY:
      break;
    case Command::EITEM:
      break;
  }

  command.PreviousCommand() = _currentCommand;
  _currentCommand           = &command;
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::endCommand(const Command& command) {
  OrkAssert(_currentCommand == &command);
  switch (command.Type()) {
    case Command::EOBJECT:
      break;
    case Command::EATTRIBUTE:
      break;
    case Command::EPROPERTY:
      break;
    case Command::EITEM:
      break;
  }
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const char& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const short& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const int& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const long& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const float& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const double& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const bool& value) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeObjectProperty(
    const ObjectProperty* prop, //
    object_constptr_t instance) {
  prop->serialize(*this, instance);
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeSharedObject(object_constptr_t instance) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::Hint(const PieceString&) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serialize(const PieceString& string) {
}
////////////////////////////////////////////////////////////////////////////////
void JsonSerializer::serializeData(const uint8_t*, size_t) {
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
