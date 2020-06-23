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

  bool Deserialize(bool&) override;
  bool Deserialize(char&) override;
  bool Deserialize(short&) override;
  bool Deserialize(int&) override;
  bool Deserialize(long&) override;
  bool Deserialize(float&) override;
  bool Deserialize(double&) override;
  bool Deserialize(rtti::ICastable*&) override;

  bool Deserialize(const IProperty*) override;
  bool Deserialize(const IObjectProperty*, Object*) override;

  bool Deserialize(MutableString&);
  bool Deserialize(ResizableString&);
  bool DeserializeData(unsigned char*, size_t) override;

  bool deserializeObject(rtti::castable_ptr_t&) = 0;

  bool ReferenceObject(rtti::ICastable*) override;
  bool BeginCommand(Command&) override;
  bool EndCommand(const Command&) override;

private:
  bool EatBinaryData();
  bool DiscardData();
  bool DiscardCommandOrData(bool& error);

  int mLineNo;
  stream::InputStreamBuffer<1024 * 4> mStream;
  orkvector<rtti::ICastable*> mDeserializedObjects;

  void EatSpace();
  void Advance(int n = 1);

  int Peek();

  bool CheckLoose(const PieceString& s, size_t& matchlen);
  bool MatchLoose(const PieceString& s);

  bool Check(const PieceString& s);
  bool Match(const PieceString& s);

  bool ReadNumber(long&);
  bool ReadNumber(double&);

  size_t ReadWord(MutableString word);
  bool MatchEndTag(const ConstString& tagname);

  bool mbReadingAttributes;
  char mAttributeEndChar;
  const Command* mCurrentCommand;

  int FindObject(rtti::ICastable* object);

  ////////////////////////////////////////////

  bool CheckExternalRead();
  bool BeginTag(const PieceString& tagname);
  bool EndTag(const PieceString& tagname);
  bool BeginAttribute(MutableString name);
  bool EndAttribute();
  bool ReadAttribute(MutableString name, MutableString value);

  template <typename StringType> bool ReadText(StringType& text);
  bool ReadBinary(unsigned char[], size_t);
  void ReadUntil(MutableString value, char terminator);
};

}}} // namespace ork::reflect::serialize
