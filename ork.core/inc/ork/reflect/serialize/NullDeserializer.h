////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/Command.h>

#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serialize {

class NullDeserializer final : public IDeserializer {
public:
  bool Deserialize(bool&) override;
  bool Deserialize(char&) override;
  bool Deserialize(short&) override;
  bool Deserialize(int&) override;
  bool Deserialize(long&) override;
  bool Deserialize(float&) override;
  bool Deserialize(double&) override;

  bool Deserialize(MutableString&) override;
  bool Deserialize(ResizableString&) override;
  bool DeserializeData(unsigned char*, size_t) override;

  bool Deserialize(const IProperty*) override;
  bool deserializeObject(rtti::castable_rawptr_t&) override;
  bool deserializeObject(rtti::castable_ptr_t&) override;
  bool deserializeObjectProperty(const IObjectProperty*, Object*) override;

  bool ReferenceObject(rtti::castable_rawptr_t) override;
  bool BeginCommand(Command&) override;
  bool EndCommand(const Command&) override;

protected:
  ///*virtual*/ bool Deserialize(const rtti::Category*, rtti::castable_rawptr_t&);
};

inline bool NullDeserializer::Deserialize(bool& value) {
  return false;
}

inline bool NullDeserializer::Deserialize(char& value) {
  return false;
}

inline bool NullDeserializer::Deserialize(short& value) {
  return false;
}

inline bool NullDeserializer::Deserialize(int& value) {
  return false;
}

inline bool NullDeserializer::Deserialize(long& value) {
  return false;
}

inline bool NullDeserializer::Deserialize(float& value) {
  return false;
}

inline bool NullDeserializer::Deserialize(double& value) {
  return false;
}

inline bool NullDeserializer::deserializeObject(rtti::castable_rawptr_t& value) {
  return false;
}
inline bool NullDeserializer::deserializeObject(rtti::castable_ptr_t& value) {
  return false;
}

inline bool NullDeserializer::Deserialize(const IProperty* prop) {
  return prop->Deserialize(*this);
}

inline bool NullDeserializer::deserializeObjectProperty(const IObjectProperty* prop, Object* object) {
  return prop->Deserialize(*this, object);
}

// inline bool NullDeserializer::Deserialize(const rtti::Category* category, rtti::castable_rawptr_t& object) {
// return category->deserializeObject(*this, object);
//}

inline bool NullDeserializer::Deserialize(MutableString& text) {
  return false;
}

inline bool NullDeserializer::Deserialize(ResizableString& text) {
  return false;
}

inline bool NullDeserializer::DeserializeData(unsigned char* data, size_t size) {
  return false;
}

inline bool NullDeserializer::ReferenceObject(rtti::castable_rawptr_t object) {
  return true;
}

inline bool NullDeserializer::BeginCommand(Command& command) {
  return true;
}

inline bool NullDeserializer::EndCommand(const Command& command) {
  return true;
}

}}} // namespace ork::reflect::serialize
