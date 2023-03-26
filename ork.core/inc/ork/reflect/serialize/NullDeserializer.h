////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/Command.h>

#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork::reflect::serdes {

class NullDeserializer final : public IDeserializer {
public:
  void deserializeTop(object_ptr_t&) override {
  }
  // void deserializeSharedObject(object_ptr_t&) override;
  // void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;
  // void referenceObject(rtti::castable_rawptr_t) override;
  // void beginCommand(Command&) override;
  // void endCommand(const Command&) override;
  // void deserializeElement() override;
};

} // namespace ork::reflect::serialize
