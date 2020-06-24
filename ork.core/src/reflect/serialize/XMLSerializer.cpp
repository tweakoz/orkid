////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/stream/IOutputStream.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/string/string.h>
#include <cstring>

namespace ork { namespace reflect { namespace serialize {

XMLSerializer::XMLSerializer(stream::IOutputStream& stream)
    : mStream(stream)
    , mIndent(0)
    , mbWritingAttributes(false)
    , mbNeedSpace(false)
    , mbNeedLine(false)
    , mCurrentCommand(NULL) {
}

bool XMLSerializer::Write(char* text, size_t size) {
  return mStream.Write(reinterpret_cast<unsigned char*>(text), size);
}

void XMLSerializer::Spaced() {
  if (mbNeedLine == false)
    mbNeedSpace = true;
}

void XMLSerializer::Unspaced() {
  mbNeedSpace = false;
}

void XMLSerializer::Lined() {
  mbNeedSpace = false;
  mbNeedLine  = true;
}

bool XMLSerializer::WriteText(const char* format, ...) {
  if (NULL == format)
    format = "";

  if (mbNeedLine) {
    mbNeedLine = false;
    WriteText("\n%*s", mIndent * 4, "");
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
  return Write(buffer, std::strlen(buffer));
}

bool XMLSerializer::StartObject(PieceString name) {
  FlushHeader();
  Lined();
  bool result = WriteText("<object type='%.*s'", name.length(), name.c_str());
  mIndent++;

  mbWritingAttributes = true;

  return result;
}

bool XMLSerializer::EndObject() {
  FlushHeader();

  Lined();
  mIndent--;
  bool result = WriteText("</object>");
  Lined();

  return result;
}

bool XMLSerializer::BeginCommand(const Command& command) {
  bool result = false;

  switch (command.Type()) {
    case Command::EOBJECT:
      result = StartObject(command.Name());
      break;
    case Command::EATTRIBUTE:
      if (mbWritingAttributes)
        result = WriteText(" %s='", command.Name().c_str());
      break;
    case Command::EPROPERTY:
      result = FlushHeader();
      Lined();
      result              = WriteText("<property name='%s'", command.Name().c_str());
      mbWritingAttributes = true;
      mIndent++;
      break;
    case Command::EITEM:
      result = FlushHeader();
      Lined();
      result              = WriteText("<item");
      mbWritingAttributes = true;
      mIndent++;
      break;
  }

  command.PreviousCommand() = mCurrentCommand;
  mCurrentCommand           = &command;

  return result;
}

bool XMLSerializer::FlushHeader() {
  bool result = true;

  if (mbWritingAttributes && (!mCurrentCommand || mCurrentCommand->Type() != Command::EATTRIBUTE)) {
    result              = WriteText(">");
    mbWritingAttributes = false;
  }

  return result;
}

bool XMLSerializer::EndCommand(const Command& command) {
  bool result(false);

  if (mCurrentCommand == &command) {
    mCurrentCommand = mCurrentCommand->PreviousCommand();
  } else {
    OrkAssertI(
        false,
        CreateFormattedString(
            "Mismatched Serializer commands! expected: %s got: %s\n",
            mCurrentCommand ? mCurrentCommand->Name().c_str() : "<no command>",
            command.Name().c_str())
            .c_str());
    return false;
  }

  switch (command.Type()) {
    case Command::EOBJECT:
      result = EndObject();
      break;
    case Command::EATTRIBUTE:
      Unspaced();
      result = WriteText("'");
      break;
    case Command::EPROPERTY:
      FlushHeader();
      mIndent--;
      Unspaced();
      result = WriteText("</property>");
      Lined();
      break;
    case Command::EITEM:
      FlushHeader();
      mIndent--;
      Unspaced();
      result = WriteText("</item>");
      Lined();
      break;
  }

  return result;
}

bool XMLSerializer::Serialize(const char& value) {
  FlushHeader();
  bool result = WriteText("%i", static_cast<unsigned char>(value));
  Spaced();
  return result;
}

bool XMLSerializer::Serialize(const short& value) {
  FlushHeader();
  bool result = WriteText("%i", value);
  Spaced();
  return result;
}

bool XMLSerializer::Serialize(const int& value) {
  FlushHeader();
  bool result = WriteText("%i", value);
  Spaced();
  return result;
}

bool XMLSerializer::Serialize(const long& value) {
  FlushHeader();
  bool result = WriteText("%i", value);
  Spaced();
  return result;
}

bool XMLSerializer::Serialize(const float& value) {
  FlushHeader();
  bool result = WriteText("%g", value);
  Spaced();
  return result;
}

bool XMLSerializer::Serialize(const double& value) {
  FlushHeader();
  bool result = WriteText("%g", value);
  Spaced();
  return result;
}

bool XMLSerializer::Serialize(const bool& value) {
  FlushHeader();

  bool result = WriteText("%s", value ? "true" : "false");
  Spaced();

  return result;
}

bool XMLSerializer::Serialize(const IProperty* prop) {
  return prop->Serialize(*this);
}

bool XMLSerializer::serializeObjectProperty(const IObjectProperty* prop, const Object* object) {
  return prop->Serialize(*this, object);
}

bool XMLSerializer::serializeObject(rtti::castable_rawconstptr_t object) {
  bool result = true;
  FlushHeader();
  if (object == NULL) {
    result = WriteText("<backreference id='-1'/>");
  } else {
    int object_index = FindObject(object);

    if (object_index != -1) {
      result = WriteText("<backreference id='%d'/>", object_index);
    } else {
      const rtti::Category* category = rtti::downcast<rtti::Category*>(object->GetClass()->GetClass());
      Lined();
      result = WriteText("<reference category='");
      result = WriteText(category->Name().c_str());
      result = WriteText("'>");
      Lined();
      mIndent++;
      if (!category->serializeObject(*this, object)) {
        result = false;
      }
      mIndent--;
      Lined();
      if (!WriteText("</reference>"))
        result = false;
      Lined();
    }
  }
  return result;
}

bool XMLSerializer::ReferenceObject(const rtti::ICastable* object) {
  OrkAssert(FindObject(object) == -1);
  int assigned_id = int(mSerializedObjects.size());
  mSerializedObjects.push_back(object);

  Command referenceAttributeCommand(Command::EATTRIBUTE, "id");
  BeginCommand(referenceAttributeCommand);
  Serialize(assigned_id);
  EndCommand(referenceAttributeCommand);

  return true;
}

int XMLSerializer::FindObject(const rtti::ICastable* object) {
  int result = -1;
  for (orkvector<const rtti::ICastable*>::size_type index = 0; index < mSerializedObjects.size(); index++) {
    // orkprintf("\tmSerializedObjects[%d] = %p\n", index, mSerializedObjects[index]);
    if (mSerializedObjects[index] == object) {
      result = int(index);
      break;
    }
  }

  // orkprintf("FindObject(%p) => %d\n", object, result);

  return result;
}

static const char* EncodeXMLAttributeChar(char c) {
  return c == '\'' ? "&apos;" : c == '&' ? "&amp;" : NULL;
}

static const char* EncodeXMLChar(char c) {
  return c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '"' ? "&quot;" : c == '&' ? "&amp;" : NULL;
}

void XMLSerializer::Hint(const PieceString&) {
}

bool XMLSerializer::Serialize(const PieceString& string) {
  FlushHeader();
  if (mCurrentCommand && mCurrentCommand->Type() == Command::EATTRIBUTE) {
    for (PieceString::size_type i = 0; i < string.length(); i++) {
      char c        = string.c_str()[i];
      const char* s = EncodeXMLAttributeChar(c);
      if (s)
        WriteText("%s", s);
      else
        WriteText("%c", c);
    }

    // return true; warning C4702: unreachable code
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

    // return true; warning C4702: unreachable code
  }

  // return false; warning C4702: unreachable code

  return true;
}

bool XMLSerializer::SerializeData(unsigned char*, size_t) {
  FlushHeader();
  return false;
}

}}} // namespace ork::reflect::serialize
