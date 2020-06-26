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

class JsonSerializer : public ISerializer {
public:
  JsonSerializer(stream::IOutputStream& stream);

  void serialize(const bool&) override;
  void serialize(const char&) override;
  void serialize(const short&) override;
  void serialize(const int&) override;
  void serialize(const long&) override;
  void serialize(const float&) override;
  void serialize(const double&) override;
  void serialize(const PieceString&) override;
  void Hint(const PieceString&) override;
  void Hint(const PieceString&, intptr_t ival) override {
  }

  void serializeSharedObject(object_constptr_t) override;
  void serializeObjectProperty(const ObjectProperty*, object_constptr_t) override;

  void serializeData(const uint8_t*, size_t size) override;

  // void referenceObject(const rtti::ICastable*) override;
  void beginCommand(const Command&) override;
  void endCommand(const Command&) override;

  // bool Serialize(const rtti::Category* category, const rtti::ICastable* object);

private:
  stream::IOutputStream& mStream;
  int mIndent;
  bool mbWritingAttributes;
  bool mbNeedSpace;
  bool mbNeedLine;
  void Spaced();
  void Lined();
  void Unspaced();

  bool Write(char* text, size_t size);
  bool WriteText(const char* format, ...);

  bool FlushHeader();

  bool StartObject(PieceString name);
  bool EndObject();
};

}}} // namespace ork::reflect::serialize
