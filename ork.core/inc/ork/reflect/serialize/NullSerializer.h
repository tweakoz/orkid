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
  void serialize(const bool&) override;
  void serialize(const char&) override;
  void serialize(const short&) override;
  void serialize(const int&) override;
  void serialize(const long&) override;
  void serialize(const float&) override;
  void serialize(const double&) override;
  void serialize(const PieceString&) override;

  void serializeObject(object_constptr_t instance) override;
  void serializeObjectProperty(
      const ObjectProperty* prop, //
      object_constptr_t instance) override;

  void Hint(const PieceString&) override;
  void Hint(const PieceString&, intptr_t ival) override;

  void serializeData(const uint8_t*, size_t) override;

  // void ReferenceObject(const rtti::ICastable*) override;

  void beginCommand(const Command&) override;
  void endCommand(const Command&) override;
};

}}} // namespace ork::reflect::serialize
