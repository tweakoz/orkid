////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/kernel/orkvector.h>
#include <ork/kernel/string/StringPool.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serialize {

class BinarySerializer final : public ISerializer {
public:
  BinarySerializer(stream::IOutputStream& stream);
  ~BinarySerializer();

  bool Serialize(const bool&) override;
  bool Serialize(const char&) override;
  bool Serialize(const short&) override;
  bool Serialize(const int&) override;
  bool Serialize(const long&) override;
  bool Serialize(const float&) override;
  bool Serialize(const double&) override;
  bool Serialize(const PieceString&) override;
  void Hint(const PieceString&) override;
  void Hint(const PieceString&, intptr_t ival) override;

  bool SerializeData(unsigned char*, size_t size) override;

  bool Serialize(const IProperty*) override;

  bool serializeObject(const rtti::ICastable*) override;
  bool serializeObjectProperty(const IObjectProperty*, const Object*) override;
  // bool serializeObjectWithCategory(
  //  const rtti::Category*, //
  // const rtti::ICastable*) override;

  bool ReferenceObject(const rtti::ICastable*) override;
  bool BeginCommand(const Command&) override;
  bool EndCommand(const Command&) override;

private:
  int FindObject(const rtti::ICastable* object);

  bool WriteHeader(char type, PieceString text);
  bool WriteFooter(char type);
  template <typename T> bool Write(const T& datum);

  stream::IOutputStream& mStream;
  orkvector<const rtti::ICastable*> mSerializedObjects;
  StringPool mStringPool;
  const Command* mCurrentCommand;
};

}}} // namespace ork::reflect::serialize
