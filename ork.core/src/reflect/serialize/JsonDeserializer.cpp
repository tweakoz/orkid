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
#include <rapidjson/reader.h>
#include <rapidjson/document.h>

namespace ork::reflect::serialize {
//////////////////////////////////////////////////////////////////////////////

static int unhex(char c);

JsonDeserializer::JsonDeserializer(stream::IInputStream& stream)
    : mStream(stream)
    , _isReadingAttributes(false)
    , mLineNo(1) {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::Advance(int n) {
  unsigned char c;

  for (int i = 0; i < n; i++) {
    mStream.Read(&c, 1);

    if (c == '\n')
      mLineNo++;
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::EatSpace() {
  while (Peek() != stream::IInputStream::kEOF && isspace(Peek()))
    Advance();
}

//////////////////////////////////////////////////////////////////////////////

size_t JsonDeserializer::ReadWord(MutableString string) {
  EatSpace();

  string = "";

  while (Peek() != stream::IInputStream::kEOF) {
    int next = Peek();
    if (isspace(next) || next == '"' || next == '\'' || next == '<' || next == '>' || next == '=') {
      break;
    } else {
      string += char(next);
      Advance();
    }
  }

  return string.size();
}

//////////////////////////////////////////////////////////////////////////////

bool JsonDeserializer::CheckExternalRead() {
  return NULL == _currentCommand || (_currentCommand->Type() == Command::EATTRIBUTE) == _isReadingAttributes;
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::ReadNumber(long& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);

  if (CheckExternalRead() && ReadWord(word) > 0) {
    char* end = 0;
    value     = std::strtol(word.c_str(), &end, 10);
    OrkAssert(end == word.c_str() + word.size());
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::ReadNumber(double& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);

  if (CheckExternalRead() && ReadWord(word) > 0) {
    char* end = 0;
    value     = std::strtod(word.c_str(), &end);
    OrkAssert(end == word.c_str() + word.size());
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(char& value) {
  bool result;
  long n;
  ReadNumber(n);
  value = char(static_cast<unsigned char>(n));
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(short& value) {
  bool result;
  long n;
  ReadNumber(n);
  value = short(n);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(int& value) {
  bool result;
  long n;
  ReadNumber(n);
  value = int(n);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(long& value) {
  bool result;
  long n;
  ReadNumber(n);
  value = n;
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(float& value) {
  bool result;
  double n;
  ReadNumber(n);
  value = float(n);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(double& value) {
  bool result;
  double n;
  ReadNumber(n);
  value = n;
}

//////////////////////////////////////////////////////////////////////////////

static bool strieq(const PieceString& a, const PieceString& b) {
  if (a.length() != b.length())
    return false;

  for (PieceString::size_type i = 0; i < a.length(); i++) {
    if (tolower(int(a.c_str()[i])) != tolower(int(b.c_str()[i]))) {
      return false;
    }
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(bool& value) {
  bool result = false;

  ArrayString<128> buffer;
  MutableString word(buffer);

  if (CheckExternalRead() && ReadWord(word) > 0) {
    if (strieq(word.c_str(), "false") || strieq(word.c_str(), "0")) {
      value  = false;
      result = true;
    } else if (strieq(word.c_str(), "true") || strieq(word.c_str(), "1")) {
      value  = true;
      result = true;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeObjectProperty(
    const ObjectProperty* prop, //
    object_ptr_t object) {
  prop->deserialize(*this, object);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeSharedObject(object_ptr_t& instance_out) {
  OrkAssert(not _isReadingAttributes);
  std::string object_uuid_str;
  //////////////////////////////////////
  ArrayString<32> attrname;
  ArrayString<128> attrvalue;
  auto TAG = nextTag();
  //////////////////////////////////////
  // backreference
  //////////////////////////////////////
  if (TAG == "backreference") {
    ReadAttribute(attrname, attrvalue);
    OrkAssert(attrname == "id");
    object_uuid_str = attrvalue;
    if (object_uuid_str == "0") {
      instance_out = nullptr;
    } else {
      boost::uuids::string_generator gen;
      auto as_uuid = gen(object_uuid_str);
      instance_out = findTrackedObject(as_uuid); //
    }

    EndTag("backreference");

  }
  //////////////////////////////////////
  // first reference
  //////////////////////////////////////
  else if (TAG == "reference") {
    ReadAttribute(attrname, attrvalue);
    OrkAssert(attrname == "id");
    boost::uuids::string_generator gen;
    auto as_uuid    = gen(object_uuid_str);
    object_uuid_str = attrvalue;

    const rtti::Category* category = NULL;

    ReadAttribute(attrname, attrvalue);
    OrkAssert(attrname == "category");
    category = rtti::downcast<const rtti::Category*>(rtti::Class::FindClass(attrvalue));
    OrkAssert(category);

    category->deserializeObject(*this, object);
    trackObject(as_uuid, object);

    bool checkendtag = EndTag("reference");
    OrkAssert(checkendtag);

  } else {
    // bad tag
    OrkAssert(false);
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(MutableString& text) {
  CheckExternalRead();

  return ReadText(text);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserialize(ResizableString& text) {
  CheckExternalRead();

  return ReadText(text);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::deserializeData(unsigned char* data, size_t size) {
  CheckExternalRead();

  return ReadBinary(data, size);
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::beginCommand(Command& command) {
  size_t checksize;
  bool result = false;

  ArrayString<32> attrname;
  ArrayString<64> attrvalue;

  EatSpace();

  OrkAssert(mStream.NumAvailable());

  if (!_isReadingAttributes) {
    if (BeginTag("property")) {
      if (ReadAttribute(attrname, attrvalue)) {
        while (attrname != "name") {
          orkprintf("JsonDeserializer::unrecognized first property attribute '%s'\n", attrname.c_str());
          if (_isReadingAttributes)
            ReadAttribute(attrname, attrvalue);
          else
            return false;
        }

        command.Setup(Command::EPROPERTY, attrvalue);
        result = true;
      }
    } else if (BeginTag("item")) {
      command.Setup(Command::EITEM);
      result = true;
    } else if (BeginTag("object")) {

      if (ReadAttribute(attrname, attrvalue)) {
        while (attrname != "type") {
          orkprintf("JsonDeserializer::unrecognized first object attribute '%s'\n", attrname.c_str());
          if (_isReadingAttributes)
            ReadAttribute(attrname, attrvalue);
          else
            return false;
        }

        command.Setup(Command::EOBJECT, attrvalue);
        result = true;
      }
    } else if (
        !CheckLoose(" < reference ", checksize) && !CheckLoose(" < backreference ", checksize) && !CheckLoose(" </ ", checksize) &&
        MatchLoose(" < ")) {
      ArrayString<256> word;
      ReadWord(word);
      OrkAssert(false);
    }
  } else if (BeginAttribute(attrname)) {
    command.Setup(Command::EATTRIBUTE, attrname);
    result = true;
  } else {
    return false;
  }

  if (result) {
    command.PreviousCommand() = _currentCommand;
    _currentCommand           = &command;
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::EatBinaryData() {
  char byte[2];
  EatSpace();
  bool result = false;

  while (stream::IInputStream::kEOF != mStream.Peek((unsigned char*)byte, sizeof(byte))) {
    if (unhex(byte[0]) == -1 || unhex(byte[1]) == -1)
      return false;

    int value = (unhex(byte[0]) << 4) + unhex(byte[1]);

    result = true;

    EatSpace();
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::endCommand(const Command& command) {
  if (_currentCommand == &command) {
    while (result == false) {
      switch (command.Type()) {
        case Command::EPROPERTY:
          result = EndTag("property");
          break;
        case Command::EATTRIBUTE:
          result = EndAttribute();
          break;
        case Command::EOBJECT:
          result = EndTag("object");
          break;
        case Command::EITEM:
          result = EndTag("item");
          break;
      }
      OrkAssert(result);
    }

    _currentCommand = _currentCommand->PreviousCommand();
  } else {
    orkprintf(
        "Mismatched Serializer commands! expected: %s got: %s\n",
        _currentCommand ? _currentCommand->Name().c_str() : "<no command>",
        command.Name().c_str());
    return false;
  }

  if (result == false) {
    orkprintf("JsonDeserializer::endCommand::[%d] failed to match end for tag '%s'\n", mLineNo, command.Name().c_str());
  }
}

//////////////////////////////////////////////////////////////////////////////

bool JsonDeserializer::Match(const PieceString& s) {
  if (Check(s)) {
    Advance(int(s.length()));
    return true;
  } else {
    return false;
  }
}

//////////////////////////////////////////////////////////////////////////////

size_t JsonDeserializer::Peek() {
  unsigned char c;
  if (mStream.Peek(&c, sizeof(c)) != stream::IInputStream::kEOF) {
    return size_t(c);
  }
  return stream::IInputStream::kEOF;
}

//////////////////////////////////////////////////////////////////////////////

bool JsonDeserializer::Check(const PieceString& s) {
  char buf[64];

  size_t bufend = minimum(s.length(), PieceString::size_type(sizeof(buf) - 1));
  mStream.Peek((unsigned char*)buf, bufend);
  buf[bufend] = '\0';

  if (std::strncmp(s.c_str(), buf, s.length()) == 0) {
    return true;
  } else {
    return false;
  }
}

//////////////////////////////////////////////////////////////////////////////

bool JsonDeserializer::MatchLoose(const PieceString& s) {
  size_t len;
  bool result = CheckLoose(s, len);

  if (result)
    Advance(int(len));

  if (result && s.c_str()[s.length() - 1] == ' ')
    EatSpace();
}

//////////////////////////////////////////////////////////////////////////////

bool JsonDeserializer::CheckLoose(const PieceString& str, size_t& matchlen) {
  PieceString s = str;
  matchlen      = 0;
  char buf[128];
  size_t bufend = size_t(mStream.Peek((unsigned char*)buf, sizeof(buf)));

  if (bufend == stream::IInputStream::kEOF)
    return false;

  while (s.length()) {
    if (s.c_str()[0] == ' ') {
      while (matchlen < bufend && isspace(buf[matchlen]))
        matchlen++;

      s = s.substr(1);
    } else {
      PieceString::size_type word_end = s.find(' ');
      PieceString word                = s.substr(0, word_end);
      s                               = s.substr(word_end);

      if (std::strncmp(buf + matchlen, word.c_str(), word.length()) == 0) {
        matchlen += word.length();
      } else {
        return false;
      }
    }
  }

  return true;
}

PieceString JsonDeserializer::nextTag() {
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::BeginTag(const PieceString& tagname) {
  OrkAssert(!_isReadingAttributes);
  EatSpace();
  ArrayString<128> buffer;
  MutableString pattern(buffer);
  pattern.format(" < %.*s ", tagname.length(), tagname.c_str());
  if (MatchLoose(pattern)) {
    _isReadingAttributes = not MatchLoose(" > ");
  } else {
    OrkAssert(false);
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::EndTag(const PieceString& tagname) {
  EatSpace();

  if (_isReadingAttributes) {
    if (MatchLoose(" /> ")) {
      _isReadingAttributes = false;
      return true;
    }
  }

  ArrayString<128> buffer;
  MutableString pattern(buffer);
  pattern.format(" </ %.*s > ", tagname.length(), tagname.c_str());

  if (MatchLoose(pattern)) {
    return true;
  } else {
    return false;
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::ReadUntil(MutableString value, char terminator) {
  char c;

  if ('\0' == terminator) {
    ReadWord(value);
    return;
  }

  while (stream::IInputStream::kEOF != mStream.Read((unsigned char*)&c, 1)) {
    if (c == '\n')
      mLineNo++;

    if (c == terminator)
      return;
    value += c;
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::BeginAttribute(MutableString name) {
  size_t checksize;
  EatSpace();

  OrkAssert(_isReadingAttributes);

  if (CheckLoose(" /> ", checksize)) {
    OrkAssert(false);
  }

  ReadWord(name);

  if (!MatchLoose(" = ")) {
    orkprintf("JsonDeserializer::BeginAttribute::[%d] expected '=' after '%s'\n", mLineNo, name.c_str());
    OrkAssert(false);
  }

  if (Match("'")) {
    mAttributeEndChar = '\'';
  } else if (Match("\"")) {
    mAttributeEndChar = '"';
  } else {
    mAttributeEndChar = '\0';
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::EndAttribute() {
  OrkAssert(_isReadingAttributes);

  EatSpace();

  if ('\'' == mAttributeEndChar)
    result = Match("'");
  if ('"' == mAttributeEndChar)
    result = Match("\"");
  if ('\0' == mAttributeEndChar)
    result = true;

  if (result && MatchLoose(" > ")) {
    _isReadingAttributes = false;
  }
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::ReadAttribute(MutableString name, MutableString value) {
  OrkAssert(_isReadingAttributes);
  BeginAttribute(name);
  ReadUntil(value, mAttributeEndChar);
  EatSpace();
  if (MatchLoose(" > ")) {
    _isReadingAttributes = false;
  }
}

//////////////////////////////////////////////////////////////////////////////

static PieceString ExpandEntity(const PieceString& entity) {
  if (entity == "amp")
    return "&";
  if (entity == "lt")
    return "<";
  if (entity == "gt")
    return ">";
  if (entity == "apos")
    return "'";
  if (entity == "quot")
    return "\"";
  return "?";
}

//////////////////////////////////////////////////////////////////////////////

template <typename StringType> //
void JsonDeserializer::ReadText(StringType& text) {
  char c;
  bool reading_attribute = _currentCommand && _currentCommand->Type() == Command::EATTRIBUTE;

  text = "";

  if (false == reading_attribute) {
    EatSpace();
    if (Peek() != '"') {
      OrkAssert(false);
    } else {
      Advance();
    }
  }

  while (stream::IInputStream::kEOF != mStream.Peek((unsigned char*)&c, 1)) {
    if (reading_attribute) {
      if (mAttributeEndChar != '\0') {
        if (c == mAttributeEndChar) {
          return;
        }
      } else {
        if (isspace(c) || '>' == c) {
          OrkAssert(0 != text.size());
          return;
        }
      }
    } else if (c == '"') {
      Advance();
      return;
    }

    if (Match("&")) {
      ArrayString<64> buffer;
      MutableString entity(buffer);
      ReadUntil(entity, ';');
      text += ExpandEntity(entity);
    } else {
      Advance();
      text += c;
    }
  }
}

template void JsonDeserializer::ReadText<MutableString>(MutableString& text);
template void JsonDeserializer::ReadText<ResizableString>(ResizableString& text);

static int unhex(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  else if ('a' <= c && c <= 'f')
    return c - 'a' + 0xA;
  else if ('A' <= c && c <= 'F')
    return c - 'A' + 0xA;
  else
    return -1;
}

//////////////////////////////////////////////////////////////////////////////

void JsonDeserializer::ReadBinary(unsigned char data[], size_t size) {
  char byte[2];
  EatSpace();

  unsigned char* edata = data + size;

  while (stream::IInputStream::kEOF != mStream.Peek((unsigned char*)byte, sizeof(byte))) {
    if (unhex(byte[0]) == -1 || unhex(byte[1]) == -1) {
      OrkAssert(false);
    }

    int value = (unhex(byte[0]) << 4) + unhex(byte[1]);

    if (data < edata) {
      *data++ = (unsigned char)value;
    } else {
      return;
    }

    EatSpace();
  }
}

//////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
