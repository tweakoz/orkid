////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>

#include <ork/orkstl.h>

namespace ork { namespace reflect { namespace serialize {

class XMLDeserializer : public IDeserializer {
public:
  XMLDeserializer(stream::IInputStream& stream);

  void deserialize(bool&) override;
  void deserialize(char&) override;
  void deserialize(short&) override;
  void deserialize(int&) override;
  void deserialize(long&) override;
  void deserialize(float&) override;
  void deserialize(double&) override;

  void deserialize(MutableString&) override;
  void deserialize(ResizableString&) override;
  void deserializeData(unsigned char*, size_t) override;

  void deserializeSharedObject(object_ptr_t&) override;
  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;

  void beginCommand(Command&) override;
  void endCommand(const Command&) override;

private:
  bool EatBinaryData();

  int mLineNo;

  stream::InputStreamBuffer<1024 * 4> mStream;

  void EatSpace();
  void Advance(int n = 1);

  size_t Peek();

  bool CheckLoose(const PieceString& s, size_t& matchlen);
  bool MatchLoose(const PieceString& s);

  bool Check(const PieceString& s);
  bool Match(const PieceString& s);

  void ReadNumber(long&);
  void ReadNumber(double&);

  size_t ReadWord(MutableString word);
  void MatchEndTag(const ConstString& tagname);

  bool _isReadingAttributes;
  char mAttributeEndChar;

  // int FindObject(rtti::ICastable* object);

  ////////////////////////////////////////////

  bool CheckExternalRead();
  bool BeginTag(const PieceString& tagname);
  bool EndTag(const PieceString& tagname);
  bool BeginAttribute(MutableString name);
  bool EndAttribute();
  bool ReadAttribute(MutableString name, MutableString value);

  template <typename StringType> void ReadText(StringType& text);
  void ReadBinary(unsigned char[], size_t);
  void ReadUntil(MutableString value, char terminator);
};

}}} // namespace ork::reflect::serialize
