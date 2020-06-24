////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>

#include <ork/orkprotos.h>
#include <cstring>

namespace ork { namespace reflect { namespace serialize {

static int unhex(char c);

XMLDeserializer::XMLDeserializer(stream::IInputStream& stream)
    : mStream(stream)
    , mbReadingAttributes(false)
    , mCurrentCommand(NULL)
    , mLineNo(1) {
}

void XMLDeserializer::Advance(int n) {
  unsigned char c;

  for (int i = 0; i < n; i++) {
    mStream.Read(&c, 1);

    if (c == '\n')
      mLineNo++;
  }
}

void XMLDeserializer::EatSpace() {
  while (Peek() != stream::IInputStream::kEOF && isspace(Peek()))
    Advance();
}

size_t XMLDeserializer::ReadWord(MutableString string) {
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

bool XMLDeserializer::CheckExternalRead() {
  return NULL == mCurrentCommand || (mCurrentCommand->Type() == Command::EATTRIBUTE) == mbReadingAttributes;
}

bool XMLDeserializer::ReadNumber(long& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);

  if (CheckExternalRead() && ReadWord(word) > 0) {
    char* end = 0;
    value     = std::strtol(word.c_str(), &end, 10);
    return end == word.c_str() + word.size();
  }

  return false;
}

bool XMLDeserializer::ReadNumber(double& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);

  if (CheckExternalRead() && ReadWord(word) > 0) {
    char* end = 0;
    value     = std::strtod(word.c_str(), &end);
    return end == word.c_str() + word.size();
  }

  return false;
}

bool XMLDeserializer::Deserialize(char& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = char(static_cast<unsigned char>(n));
  return result;
}

bool XMLDeserializer::Deserialize(short& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = short(n);
  return result;
}

bool XMLDeserializer::Deserialize(int& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = int(n);
  return result;
}

bool XMLDeserializer::Deserialize(long& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = n;
  return result;
}

bool XMLDeserializer::Deserialize(float& value) {
  bool result;
  double n;
  result = ReadNumber(n);
  value  = float(n);
  return result;
}

bool XMLDeserializer::Deserialize(double& value) {
  bool result;
  double n;
  result = ReadNumber(n);
  value  = n;
  return result;
}

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

bool XMLDeserializer::Deserialize(bool& value) {
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

  return result;
}

bool XMLDeserializer::Deserialize(const IProperty* prop) {
  return prop->Deserialize(*this);
}

bool XMLDeserializer::deserializeObjectProperty(const IObjectProperty* prop, Object* object) {
  return prop->Deserialize(*this, object);
}

bool XMLDeserializer::ReferenceObject(rtti::castable_rawptr_t object) {
  int object_id = -1;
  Command referenceAttributeCommand;

  if (mbReadingAttributes) {
    bool result = false;

    if (BeginCommand(referenceAttributeCommand)) {
      OrkAssert(referenceAttributeCommand.Type() == Command::EATTRIBUTE);
      OrkAssert(referenceAttributeCommand.Name() == "id");

      if (referenceAttributeCommand.Type() != Command::EATTRIBUTE)
        return false;

      if (referenceAttributeCommand.Name() != "id")
        return false;

      result = Deserialize(object_id);
      EndCommand(referenceAttributeCommand);
    }

    OrkAssert(result);
    if (result == false)
      return false;
  }

  int assigned_id = int(_reftracker.size());
  _reftracker.push_back(object);

  if (object_id != -1) {
    OrkAssert(assigned_id == object_id);
    if (assigned_id != object_id)
      return false;
  }

  return true;
}

template <typename ptrtype, typename dmtype> //
bool XMLDeserializer::_deserializeObject(
    ptrtype& object, //
    dmtype& dmethod) {

  if (mbReadingAttributes)
    return false;

  OrkAssert(!mbReadingAttributes);

  if (BeginTag("backreference")) {
    ArrayString<32> attrname;
    ArrayString<32> attrvalue;

    if (!ReadAttribute(attrname, attrvalue)) {
      while (attrname != "id") {
        orkprintf("XMLDeserializer:: <backreference ... expected id attribute\n");
        if (false == ReadAttribute(attrname, attrvalue)) {
          return false;
        }
      }
    }

    int object_id = std::atoi(attrvalue.c_str());

    if (object_id == -1) {
      object = NULL;
    } else if (object_id < int(_reftracker.size())) {
      auto index     = trackervect_t::size_type(object_id);
      auto rawobject = _reftracker[index];
      // object         = ;
      OrkAssert(false);
      // we need to paramterize object = rawobject; on ptrtype somehow
      //  or we will need separate _deserializeObject methods for raw and sharedptr
    } else {
      ArrayString<32> buffer;
      MutableString error(buffer);
      error.format("backreference %d not available!", object_id);
      OrkAssertI(false, error.c_str());
    }

    if (!EndTag("backreference")) {
      orkprintf("XMLDeserializer:: expected </backreference>\n");
      return false;
    }

    return true;
  } else if (BeginTag("reference")) {

    const rtti::Category* category = NULL;

    if (mbReadingAttributes) {
      ArrayString<32> attrname;
      ArrayString<128> attrvalue;

      ReadAttribute(attrname, attrvalue);

      while (attrname != "category") {
        orkprintf("XMLDeserializer:: <reference ... unknown attribute '%s'\n", attrname.c_str());
        if (mbReadingAttributes) {
          ReadAttribute(attrname, attrvalue);
        } else {
          return false;
        }
      }

      category = rtti::downcast<const rtti::Category*>(rtti::Class::FindClass(attrvalue));

      if (category == NULL) {
        orkprintf("XMLDeserializer:: <reference ... unknown category='%s'\n", attrvalue.c_str());
        return false;
      }
    } else {
      orkprintf("XMLDeserializer:: <reference ... expected 'category' attribute\n");
      return false;
    }

    if (false == category->deserializeObject(*this, object)) {
      return false;
    }

    if (!EndTag("reference")) {
      orkprintf("XMLDeserializer:: expected </reference>\n");
      return false;
    }

    return true;
  } else {
    // orkprintf("XMLDeserializer:: expected <reference> or <backreference>\n");
    return false;
  }
}

bool XMLDeserializer::deserializeObject(rtti::castable_rawptr_t& object) {
  // void (C::* p)(int) = &C::f;
  using dmethod_t = bool (rtti::Category::*)(IDeserializer&, rtti::castable_rawptr_t&) const;
  dmethod_t dm    = &rtti::Category::deserializeObject;
  return _deserializeObject(object, dm);
}
bool XMLDeserializer::deserializeObject(rtti::castable_ptr_t& object) {
  using dmethod_t = bool (rtti::Category::*)(IDeserializer&, rtti::castable_ptr_t&) const;
  dmethod_t dm    = &rtti::Category::deserializeObject;
  return _deserializeObject(object, dm);
}

bool XMLDeserializer::Deserialize(MutableString& text) {
  CheckExternalRead();

  return ReadText(text);
}

bool XMLDeserializer::Deserialize(ResizableString& text) {
  CheckExternalRead();

  return ReadText(text);
}

bool XMLDeserializer::DeserializeData(unsigned char* data, size_t size) {
  CheckExternalRead();

  return ReadBinary(data, size);
}

bool XMLDeserializer::BeginCommand(Command& command) {
  size_t checksize;
  bool result = false;

  ArrayString<32> attrname;
  ArrayString<64> attrvalue;

  EatSpace();

  // if( mStream.NumAvailable() == 0 )
  //{
  //	return false;
  //}

  if (!mbReadingAttributes) {
    if (BeginTag("property")) {
      if (ReadAttribute(attrname, attrvalue)) {
        while (attrname != "name") {
          orkprintf("XMLDeserializer::unrecognized first property attribute '%s'\n", attrname.c_str());
          if (mbReadingAttributes)
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
          orkprintf("XMLDeserializer::unrecognized first object attribute '%s'\n", attrname.c_str());
          if (mbReadingAttributes)
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
      orkprintf("XMLDeserializer::BeginCommand::[%d] unknown tag %s\n", mLineNo, word.c_str());
      return false;
    }
  } else if (BeginAttribute(attrname)) {
    command.Setup(Command::EATTRIBUTE, attrname);
    result = true;
  } else {
    return false;
  }

  if (result) {
    command.PreviousCommand() = mCurrentCommand;
    mCurrentCommand           = &command;
  }

  return result;
}

bool XMLDeserializer::EatBinaryData() {
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

  return result;
}

bool XMLDeserializer::EndCommand(const Command& command) {
  bool result = false;

  if (mCurrentCommand == &command) {
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

    mCurrentCommand = mCurrentCommand->PreviousCommand();
  } else {
    orkprintf(
        "Mismatched Serializer commands! expected: %s got: %s\n",
        mCurrentCommand ? mCurrentCommand->Name().c_str() : "<no command>",
        command.Name().c_str());
    return false;
  }

  if (result == false) {
    orkprintf("XMLDeserializer::EndCommand::[%d] failed to match end for tag '%s'\n", mLineNo, command.Name().c_str());
  }

  return result;
}

bool XMLDeserializer::Match(const PieceString& s) {
  if (Check(s)) {
    Advance(int(s.length()));
    return true;
  } else {
    return false;
  }
}

size_t XMLDeserializer::Peek() {
  unsigned char c;
  if (mStream.Peek(&c, sizeof(c)) != stream::IInputStream::kEOF) {
    return size_t(c);
  }
  return stream::IInputStream::kEOF;
}

bool XMLDeserializer::Check(const PieceString& s) {
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

bool XMLDeserializer::MatchLoose(const PieceString& s) {
  size_t len;
  bool result = CheckLoose(s, len);

  if (result)
    Advance(int(len));

  if (result && s.c_str()[s.length() - 1] == ' ')
    EatSpace();

  return result;
}

bool XMLDeserializer::CheckLoose(const PieceString& str, size_t& matchlen) {
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
// yp
bool XMLDeserializer::BeginTag(const PieceString& tagname) {
  OrkAssert(!mbReadingAttributes);

  EatSpace();
  ArrayString<128> buffer;
  MutableString pattern(buffer);
  pattern.format(" < %.*s ", tagname.length(), tagname.c_str());

  if (MatchLoose(pattern)) {
    if (MatchLoose(" > "))
      mbReadingAttributes = false;
    else
      mbReadingAttributes = true;

    return true;
  } else {
    return false;
  }
}

bool XMLDeserializer::EndTag(const PieceString& tagname) {
  EatSpace();

  if (mbReadingAttributes) {
    if (MatchLoose(" /> ")) {
      mbReadingAttributes = false;
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

void XMLDeserializer::ReadUntil(MutableString value, char terminator) {
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

bool XMLDeserializer::BeginAttribute(MutableString name) {
  size_t checksize;
  EatSpace();

  OrkAssert(mbReadingAttributes);

  if (CheckLoose(" /> ", checksize)) {
    return false;
  }

  ReadWord(name);

  if (!MatchLoose(" = ")) {
    orkprintf("XMLDeserializer::BeginAttribute::[%d] expected '=' after '%s'\n", mLineNo, name.c_str());
    return false;
  }

  if (Match("'")) {
    mAttributeEndChar = '\'';
  } else if (Match("\"")) {
    mAttributeEndChar = '"';
  } else {
    mAttributeEndChar = '\0';
  }

  return true;
}

bool XMLDeserializer::EndAttribute() {
  bool result = false;

  OrkAssert(mbReadingAttributes);

  EatSpace();

  if ('\'' == mAttributeEndChar)
    result = Match("'");
  if ('"' == mAttributeEndChar)
    result = Match("\"");
  if ('\0' == mAttributeEndChar)
    result = true;

  if (result && MatchLoose(" > ")) {
    mbReadingAttributes = false;
  }

  return result;
}

bool XMLDeserializer::ReadAttribute(MutableString name, MutableString value) {
  if (BeginAttribute(name)) {
    ReadUntil(value, mAttributeEndChar);

    EatSpace();

    if (MatchLoose(" > ")) {
      mbReadingAttributes = false;
    }

    return true;
  }

  return false;
}

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

template <typename StringType> bool XMLDeserializer::ReadText(StringType& text) {
  char c;
  bool reading_attribute = mCurrentCommand && mCurrentCommand->Type() == Command::EATTRIBUTE;

  text = "";

  if (false == reading_attribute) {
    EatSpace();
    if (Peek() != '"') {
      return false;
    } else {
      Advance();
    }
  }

  while (stream::IInputStream::kEOF != mStream.Peek((unsigned char*)&c, 1)) {
    if (reading_attribute) {
      if (mAttributeEndChar != '\0') {
        if (c == mAttributeEndChar) {
          return true;
        }
      } else {
        if (isspace(c) || '>' == c) {
          return 0 != text.size();
        }
      }
    } else if (c == '"') {
      Advance();
      return true;
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

  return true;
}

template bool XMLDeserializer::ReadText<MutableString>(MutableString& text);
template bool XMLDeserializer::ReadText<ResizableString>(ResizableString& text);

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

bool XMLDeserializer::ReadBinary(unsigned char data[], size_t size) {
  char byte[2];
  EatSpace();

  unsigned char* edata = data + size;

  while (stream::IInputStream::kEOF != mStream.Peek((unsigned char*)byte, sizeof(byte))) {
    if (unhex(byte[0]) == -1 || unhex(byte[1]) == -1)
      return false;

    int value = (unhex(byte[0]) << 4) + unhex(byte[1]);

    if (data < edata) {
      *data++ = (unsigned char)value;
    } else {
      return true;
    }

    EatSpace();
  }

  return true;
}

}}} // namespace ork::reflect::serialize
