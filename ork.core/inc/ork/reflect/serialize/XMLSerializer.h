////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/orkstl.h>
#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serialize {

class XMLSerializer : public ISerializer {
public:
  XMLSerializer(stream::IOutputStream& stream);

  bool Serialize(const bool&) override;
  bool Serialize(const char&) override;
  bool Serialize(const short&) override;
  bool Serialize(const int&) override;
  bool Serialize(const long&) override;
  bool Serialize(const float&) override;
  bool Serialize(const double&) override;
  bool Serialize(const PieceString&) override;
  void Hint(const PieceString&) override;
  void Hint(const PieceString&, intptr_t ival) override {
  }

  bool serializeObject(rtti::castable_rawconstptr_t) override;
  bool serializeObjectProperty(const ObjectProperty*, const Object*) override;
  bool Serialize(const AbstractProperty*) override;

  bool SerializeData(unsigned char*, size_t size) override;

  bool ReferenceObject(const rtti::ICastable*) override;
  bool BeginCommand(const Command&) override;
  bool EndCommand(const Command&) override;

  bool Serialize(const rtti::Category* category, const rtti::ICastable* object);

private:
  stream::IOutputStream& mStream;
  orkvector<const rtti::ICastable*> mSerializedObjects;
  int mIndent;
  bool mbWritingAttributes;
  bool mbNeedSpace;
  bool mbNeedLine;
  const Command* mCurrentCommand;
  void Spaced();
  void Lined();
  void Unspaced();

  bool Write(char* text, size_t size);
  bool WriteText(const char* format, ...);

  bool FlushHeader();

  bool StartObject(PieceString name);
  bool EndObject();
  int FindObject(const rtti::ICastable*);
};

}}} // namespace ork::reflect::serialize
