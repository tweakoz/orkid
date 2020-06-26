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

namespace ork::reflect::serialize {

class NullDeserializer final : public IDeserializer {
public:
  void deserialize(bool&) override;
  void deserialize(char&) override;
  void deserialize(short&) override;
  void deserialize(int&) override;
  void deserialize(long&) override;
  void deserialize(float&) override;
  void deserialize(double&) override;

  void deserialize(MutableString&) override;
  void deserialize(ResizableString&) override;
  void deserializeData(unsigned char*, size_t) override;

  void deserializeSharedObject(object_ptr_t&) override;
  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;

  void referenceObject(rtti::castable_rawptr_t) override;
  void beginCommand(Command&) override;
  void endCommand(const Command&) override;
};

inline void NullDeserializer::deserialize(bool& value) {
}

inline void NullDeserializer::deserialize(char& value) {
}

inline void NullDeserializer::deserialize(short& value) {
}

inline void NullDeserializer::deserialize(int& value) {
}

inline void NullDeserializer::deserialize(long& value) {
}

inline void NullDeserializer::deserialize(float& value) {
}

inline void NullDeserializer::deserialize(double& value) {
}

inline void NullDeserializer::deserializeSharedObject(object_ptr_t& value_out) {
}

inline void NullDeserializer::deserializeObjectProperty(const ObjectProperty* prop, object_ptr_t object) {
  prop->Deserialize(*this, object);
}

inline void NullDeserializer::deserialize(MutableString& text) {
}

inline void NullDeserializer::deserialize(ResizableString& text) {
}

inline void NullDeserializer::deserializeData(unsigned char* data, size_t size) {
}

inline void NullDeserializer::referenceObject(rtti::castable_rawptr_t object) {
}

inline void NullDeserializer::beginCommand(Command& command) {
}

inline void NullDeserializer::endCommand(const Command& command) {
}

} // namespace ork::reflect::serialize
