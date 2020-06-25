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

  bool Deserialize(MutableString&) override;
  bool Deserialize(ResizableString&) override;
  bool DeserializeData(unsigned char*, size_t) override;

  bool Deserialize(const IProperty*) override;
  bool deserializeObject(rtti::castable_rawptr_t&) override;
  bool deserializeSharedObject(rtti::castable_ptr_t&) override;
  bool deserializeObjectProperty(const I*, Object*) override;

  template <
      typename ptrtype, //
      typename dmtype>  //
  bool _deserializeObject(
      ptrtype&, //
      dmtype&);

  bool ReferenceObject(rtti::ICastable*) override;
  bool BeginCommand(Command&) override;
  bool EndCommand(const Command&) override;

private:
  bool EatBinaryData();

  int mLineNo;

  using trackervect_t = orkvector<rtti::castable_rawptr_t>;

  stream::InputStreamBuffer<1024 * 4> mStream;
  trackervect_t _reftracker;
  // orkvector<rtti::castable_ptr_t> _deserializedSHARED;

  void EatSpace();
  void Advance(int n = 1);

  size_t Peek();

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
