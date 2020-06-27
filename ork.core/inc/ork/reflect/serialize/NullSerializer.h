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

struct NullSerializer final : public ISerializer {

  void serializeItem(const hintvar_t&) override;
  void serializeSharedObject(object_constptr_t instance) override;
  void serializeObjectProperty(
      const ObjectProperty* prop, //
      object_constptr_t instance) override;

  void Hint(const PieceString&, hintvar_t val) override;

  void serializeData(const uint8_t*, size_t) override;

  // void ReferenceObject(const rtti::ICastable*) override;

  void beginCommand(const Command&) override;
  void endCommand(const Command&) override;
};

}}} // namespace ork::reflect::serialize
