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

struct BinarySerializer final : public ISerializer {

  BinarySerializer(stream::IOutputStream& stream);
  ~BinarySerializer();

  void serialize(const bool&) override;
  void serialize(const char&) override;
  void serialize(const short&) override;
  void serialize(const int&) override;
  void serialize(const long&) override;
  void serialize(const float&) override;
  void serialize(const double&) override;
  void serialize(const PieceString&) override;
  void Hint(const PieceString&) override;
  void Hint(const PieceString&, intptr_t ival) override;

  void serializeData(const uint8_t*, size_t size) override;

  // void serialize(const AbstractProperty*) override;

  void serializeSharedObject(object_constptr_t) override;
  void serializeObjectProperty(const ObjectProperty*, object_constptr_t) override;

  // void ReferenceObject(const rtti::ICastable*) override;
  void beginCommand(const Command&) override;
  void endCommand(const Command&) override;

private:
  void _writeHeader(char type, PieceString text);
  void _writeFooter(char type);
  template <typename T> void _write(const T& datum);

  stream::IOutputStream& mStream;
  StringPool mStringPool;
};

}}} // namespace ork::reflect::serialize
