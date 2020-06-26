////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/TextDeserializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
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

void TextDeserializer::ReadWord(MutableString string) {
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
}

void TextDeserializer::ReadNumber(long& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);
  ReadWord(word);
  value = std::atoi(word.c_str());
}

void TextDeserializer::ReadNumber(double& value) {
  ArrayString<128> buffer;
  MutableString word(buffer);
  ReadWord(word);
  value = std::atof(word.c_str());
}

void TextDeserializer::deserialize(char& value) {
  long n;
  ReadNumber(n);
  value = char(static_cast<unsigned char>(n));
}

void TextDeserializer::deserialize(short& value) {
  long n;
  ReadNumber(n);
  value = short(n);
}

void TextDeserializer::deserialize(int& value) {
  long n;
  ReadNumber(n);
  value = int(n);
}

void TextDeserializer::deserialize(long& value) {
  long n;
  ReadNumber(n);
  value = n;
}

void TextDeserializer::deserialize(float& value) {
  double n;
  ReadNumber(n);
  value = float(n);
}

void TextDeserializer::deserialize(double& value) {
  double n;
  ReadNumber(n);
  value = n;
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

void TextDeserializer::deserialize(bool& value) {

  ArrayString<128> buffer;
  MutableString word(buffer);

  ReadWord(word);
  if (strieq(word.c_str(), "false") or strieq(word.c_str(), "0"))
    value = false;
  else if (strieq(word.c_str(), "true") or strieq(word.c_str(), "1")) {
    value = true;
  }
}

void TextDeserializer::deserializeObjectProperty(
    const ObjectProperty* prop, //
    object_ptr_t object) {
  prop->deserialize(*this, object);
}

void TextDeserializer::deserializeSharedObject(object_ptr_t& instance_out) {
  ArrayString<128> buffer;
  MutableString word(buffer);
  ReadWord(word);
  void* pdata = 0;
  sscanf(word.c_str(), "%p", &pdata);
  OrkAssert(false); // instance_out;
}

void TextDeserializer::deserialize(MutableString& text) {
  while (Peek() != stream::IInputStream::kEOF and isspace(Peek())) {
    text += char(Peek());
    Advance();
  }
}

void TextDeserializer::deserialize(ResizableString& text) {
  while (Peek() != stream::IInputStream::kEOF and isspace(Peek())) {
    text += char(Peek());
    Advance();
  }
}

void TextDeserializer::deserializeData(unsigned char* data, size_t size) {
}

void TextDeserializer::beginCommand(Command& command) {
}

void TextDeserializer::endCommand(const Command& command) {
}

size_t TextDeserializer::Peek() {
  unsigned char c;

  if (mStream.Peek(&c, sizeof(c)) != stream::IInputStream::kEOF) {
    return size_t(c);
  }

  return stream::IInputStream::kEOF;
}
}}} // namespace ork::reflect::serialize
