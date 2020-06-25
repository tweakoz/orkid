////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h> // for base

namespace ork { namespace rtti {
class Category;
}} // namespace ork::rtti

namespace ork { namespace reflect { namespace serialize {

class NullSerializer final : public ISerializer {
public:
  bool Serialize(const bool&) override;
  bool Serialize(const char&) override;
  bool Serialize(const short&) override;
  bool Serialize(const int&) override;
  bool Serialize(const long&) override;
  bool Serialize(const float&) override;
  bool Serialize(const double&) override;
  bool Serialize(const PieceString&) override;

  bool serializeObject(const rtti::ICastable*) override;
  // bool serializeObjectWithCategory(
  //  const rtti::Category* cat, //
  // const rtti::ICastable* object) override;
  bool serializeObjectProperty(
      const I* prop, //
      const Object* object) override;

  bool Serialize(const IProperty* prop) override;

  void Hint(const PieceString&) override;
  void Hint(const PieceString&, intptr_t ival) override;

  bool SerializeData(unsigned char*, size_t) override;

  bool ReferenceObject(const rtti::ICastable*) override;

  bool BeginCommand(const Command&) override;
  bool EndCommand(const Command&) override;
};

}}} // namespace ork::reflect::serialize
