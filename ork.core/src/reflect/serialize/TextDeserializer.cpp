////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/TextDeserializer.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/properties/I.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>

namespace ork { namespace reflect { namespace serialize {

TextDeserializer::TextDeserializer(stream::IInputStream& stream)
    : mStream(stream) {
}

void TextDeserializer::Advance() {
  unsigned char c;
  mStream.Read(&c, 1);
}

void TextDeserializer::EatSpace() {
  while (Peek() != stream::IInputStream::kEOF and isspace(Peek()))
    Advance();
}

size_t TextDeserializer::ReadWord(MutableString string) {
  EatSpace();

  string = "";

  while (Peek() != stream::IInputStream::kEOF) {
    int next = Peek();
    if (isspace(next) or //
        next == '"' or   //
        next == '\'' or  //
        next == '<' or   //
        next == '>' or   //
        next == '=') {
      break;
    } else {
      string += char(next);
      Advance();
    }
  }

  return true;
}

bool TextDeserializer::ReadNumber(long& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);

  if (ReadWord(word) > 0) {
    value = std::atoi(word.c_str());
    return true;
  }

  return false;
}

bool TextDeserializer::ReadNumber(double& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);

  if (ReadWord(word) > 0) {
    value = std::atof(word.c_str());
    return true;
  }

  return false;
}

bool TextDeserializer::Deserialize(char& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = char(static_cast<unsigned char>(n));
  return result;
}

bool TextDeserializer::Deserialize(short& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = short(n);
  return result;
}

bool TextDeserializer::Deserialize(int& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = int(n);
  return result;
}

bool TextDeserializer::Deserialize(long& value) {
  bool result;
  long n;
  result = ReadNumber(n);
  value  = n;
  return result;
}

bool TextDeserializer::Deserialize(float& value) {
  bool result;
  double n;
  result = ReadNumber(n);
  value  = float(n);
  return result;
}

bool TextDeserializer::Deserialize(double& value) {
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

bool TextDeserializer::Deserialize(bool& value) {

  bool result = false;

  ArrayString<128> buffer;
  MutableString word(buffer);

  if (ReadWord(word) > 0) {
    if (strieq(word.c_str(), "false") or strieq(word.c_str(), "0")) {
      value  = false;
      result = true;
    } else if (strieq(word.c_str(), "true") or strieq(word.c_str(), "1")) {
      value  = true;
      result = true;
    }
  }

  return result;
}

bool TextDeserializer::Deserialize(const IProperty* prop) {
  return prop->Deserialize(*this);
}

bool TextDeserializer::deserializeObjectProperty(const ObjectProperty* prop, Object* object) {
  return prop->Deserialize(*this, object);
}

bool TextDeserializer::ReferenceObject(rtti::castable_rawptr_t object) {
  return false;
}

bool TextDeserializer::deserializeObject(rtti::castable_rawptr_t& object) {
  ArrayString<128> buffer;
  MutableString word(buffer);
  if (ReadWord(word) > 0) {
    void* pdata = 0;
    sscanf(word.c_str(), "%p", &object);
    return true;
  }
  return false;
}
bool TextDeserializer::deserializeSharedObject(rtti::castable_ptr_t& object) {
  OrkAssert(false);
  return false;
}

bool TextDeserializer::Deserialize(MutableString& text) {
  while (Peek() != stream::IInputStream::kEOF and isspace(Peek())) {
    text += char(Peek());
    Advance();
  }
  return true;
}

bool TextDeserializer::Deserialize(ResizableString& text) {
  while (Peek() != stream::IInputStream::kEOF and isspace(Peek())) {
    text += char(Peek());
    Advance();
  }
  return true;
}

bool TextDeserializer::DeserializeData(unsigned char* data, size_t size) {
  return false;
}

bool TextDeserializer::BeginCommand(Command& command) {
  return false;
}

bool TextDeserializer::EndCommand(const Command& command) {
  return false;
}

size_t TextDeserializer::Peek() {
  unsigned char c;

  if (mStream.Peek(&c, sizeof(c)) != stream::IInputStream::kEOF) {
    return size_t(c);
  }

  return stream::IInputStream::kEOF;
}

}}} // namespace ork::reflect::serialize
