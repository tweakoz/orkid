////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
