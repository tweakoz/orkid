////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/stream/IOutputStream.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/string/string.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>
#include <cstring>

namespace ork::reflect::serialize {
////////////////////////////////////////////////////////////////////////////////
XMLSerializer::XMLSerializer(stream::IOutputStream& stream)
    : mStream(stream)
    , mIndent(0)
    , mbWritingAttributes(false)
    , mbNeedSpace(false)
    , mbNeedLine(false)
    , _currentCommand(NULL) {
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Write(char* text, size_t size) {
  bool status = mStream.Write(reinterpret_cast<unsigned char*>(text), size);
  OrkAssert(status);
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Spaced() {
  if (mbNeedLine == false)
    mbNeedSpace = true;
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Unspaced() {
  mbNeedSpace = false;
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Lined() {
  mbNeedSpace = false;
  mbNeedLine  = true;
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::WriteText(const char* format, ...) {
  if (NULL == format)
    format = "";

  if (mbNeedLine) {
    mbNeedLine = false;
    WriteText("\n%*s", mIndent, "");
  }

  if (mbNeedSpace) {
    mbNeedSpace = false;
    WriteText(" ");
  }

  char buffer[4096];
  va_list list;
  va_start(list, format);
  vsprintf(buffer, /*sizeof(buffer),*/ format, list);
  va_end(list);
  buffer[sizeof(buffer) - 1] = '\0';
  Write(buffer, std::strlen(buffer));
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::StartObject(PieceString name) {
  FlushHeader();
  Lined();
  WriteText("<object type='%.*s'", name.length(), name.c_str());
  mIndent++;
  mbWritingAttributes = true;
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::EndObject() {
  FlushHeader();
  Lined();
  mIndent--;
  WriteText("</object>");
  Lined();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::beginCommand(const Command& command) {
  switch (command.Type()) {
    case Command::EOBJECT:
      StartObject(command.Name());
      break;
    case Command::EATTRIBUTE:
      if (mbWritingAttributes)
        WriteText(" %s='", command.Name().c_str());
      break;
    case Command::EPROPERTY:
      FlushHeader();
      Lined();
      result              = WriteText("<property name='%s'", command.Name().c_str());
      mbWritingAttributes = true;
      mIndent++;
      break;
    case Command::EITEM:
      FlushHeader();
      Lined();
      result              = WriteText("<item");
      mbWritingAttributes = true;
      mIndent++;
      break;
  }

  command.PreviousCommand() = _currentCommand;
  _currentCommand           = &command;
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::FlushHeader() {
  if (mbWritingAttributes && (!_currentCommand || _currentCommand->Type() != Command::EATTRIBUTE)) {
    result              = WriteText(">");
    mbWritingAttributes = false;
  }
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::endCommand(const Command& command) {
  if (_currentCommand == &command) {
    _currentCommand = _currentCommand->PreviousCommand();
  } else {
    OrkAssertI(
        false,
        CreateFormattedString(
            "Mismatched Serializer commands! expected: %s got: %s\n",
            _currentCommand ? _currentCommand->Name().c_str() : "<no command>",
            command.Name().c_str())
            .c_str());
  }
  switch (command.Type()) {
    case Command::EOBJECT:
      EndObject();
      break;
    case Command::EATTRIBUTE:
      Unspaced();
      WriteText("'");
      break;
    case Command::EPROPERTY:
      FlushHeader();
      mIndent--;
      Unspaced();
      WriteText("</property>");
      Lined();
      break;
    case Command::EITEM:
      FlushHeader();
      mIndent--;
      Unspaced();
      WriteText("</item>");
      Lined();
      break;
  }
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const char& value) {
  FlushHeader();
  WriteText("%i", static_cast<unsigned char>(value));
  Spaced();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const short& value) {
  FlushHeader();
  WriteText("%i", value);
  Spaced();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const int& value) {
  FlushHeader();
  WriteText("%i", value);
  Spaced();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const long& value) {
  FlushHeader();
  WriteText("%i", value);
  Spaced();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const float& value) {
  FlushHeader();
  WriteText("%g", value);
  Spaced();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const double& value) {
  FlushHeader();
  WriteText("%g", value);
  Spaced();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const bool& value) {
  FlushHeader();
  WriteText("%s", value ? "true" : "false");
  Spaced();
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const AbstractProperty* prop) {
  prop->Serialize(*this);
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::serializeObjectProperty(const ObjectProperty* prop, const Object* object) {
  prop->Serialize(*this, object);
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::serializeObject(rtti::castable_rawconstptr_t castable) {
  auto as_object = dynamic_cast<const ork::Object*>(castable);
  FlushHeader();
  if (as_object == nullptr) {
    WriteText("<backreference id='0'/>");
  } else {
    const auto& uuid  = as_object->_uuid;
    std::string uuids = boost::uuids::to_string(uuid);

    auto it = _serialized.find(uuids);

    if (it != _serialized.end()) {
      WriteText("<backreference id='%s'/>", uuids.c_str());
    } else {
      auto objclazz = as_object->GetClass();
      auto category = rtti::downcast<rtti::Category*>(objclazz->GetClass());
      Lined();
      WriteText("<reference category='");
      WriteText(category->Name().c_str());
      WriteText("'>");
      Lined();
      mIndent++;
      // printf("xmlser obj<%p>\n", object);
      category->serializeObject(*this, as_object);
      mIndent--;
      Lined();
      WriteText("</reference>");
      Lined();
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
static const char* EncodeXMLAttributeChar(char c) {
  return c == '\'' ? "&apos;" : c == '&' ? "&amp;" : NULL;
}
////////////////////////////////////////////////////////////////////////////////
static const char* EncodeXMLChar(char c) {
  return c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '"' ? "&quot;" : c == '&' ? "&amp;" : NULL;
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Hint(const PieceString&) {
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::Serialize(const PieceString& string) {
  FlushHeader();
  if (_currentCommand and //
      _currentCommand->Type() == Command::EATTRIBUTE) {
    for (size_t i = 0; i < string.length(); i++) {
      char c        = string.c_str()[i];
      const char* s = EncodeXMLAttributeChar(c);
      if (s)
        WriteText("%s", s);
      else
        WriteText("%c", c);
    }
  } else {
    WriteText("\"");
    for (PieceString::size_type i = 0; i < string.length(); i++) {
      char c = string.c_str()[i];

      const char* s = EncodeXMLChar(c);
      if (s)
        WriteText("%s", s);
      else
        WriteText("%c", c);
    }
    WriteText("\"");
    Spaced();
  }
}
////////////////////////////////////////////////////////////////////////////////
void XMLSerializer::SerializeData(unsigned char*, size_t) {
  FlushHeader();
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
