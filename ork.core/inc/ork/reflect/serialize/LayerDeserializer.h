////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/properties/I.h>
#include <ork/reflect/Command.h>

#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serialize {

class LayerDeserializer : public IDeserializer {
public:
  LayerDeserializer(IDeserializer& deserializer);

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
  bool deserializeSharedObject(rtti::castable_ptr_t&) override;
  bool deserializeObjectProperty(const ObjectProperty*, Object*) override;

  bool ReferenceObject(rtti::castable_rawptr_t) override;
  bool BeginCommand(Command&) override;
  bool EndCommand(const Command&) override;

protected:
  bool _deserialize(const rtti::Category*, rtti::castable_rawptr_t&);

protected:
  IDeserializer& mDeserializer;
  const Command* mCurrentCommand;
};

inline LayerDeserializer::LayerDeserializer(IDeserializer& deserializer)
    : mDeserializer(deserializer)
    , mCurrentCommand(NULL) {
}

inline bool LayerDeserializer::Deserialize(bool& value) {
  return mDeserializer.Deserialize(value);
}

inline bool LayerDeserializer::Deserialize(char& value) {
  return mDeserializer.Deserialize(value);
}

inline bool LayerDeserializer::Deserialize(short& value) {
  return mDeserializer.Deserialize(value);
}

inline bool LayerDeserializer::Deserialize(int& value) {
  return mDeserializer.Deserialize(value);
}

inline bool LayerDeserializer::Deserialize(long& value) {
  return mDeserializer.Deserialize(value);
}

inline bool LayerDeserializer::Deserialize(float& value) {
  return mDeserializer.Deserialize(value);
}

inline bool LayerDeserializer::Deserialize(double& value) {
  return mDeserializer.Deserialize(value);
}

inline bool LayerDeserializer::deserializeObject(rtti::castable_rawptr_t& value) {
  return mDeserializer.deserializeObject(value);
}
inline bool LayerDeserializer::deserializeSharedObject(rtti::castable_ptr_t& value) {
  return mDeserializer.deserializeSharedObject(value);
}

inline bool LayerDeserializer::Deserialize(const IProperty* prop) {
  return prop->Deserialize(*this);
}

inline bool LayerDeserializer::deserializeObjectProperty(const ObjectProperty* prop, Object* object) {
  return prop->Deserialize(*this, object);
}

inline bool LayerDeserializer::_deserialize(const rtti::Category* category, rtti::castable_rawptr_t& object) {
  return category->deserializeObject(*this, object);
}

inline bool LayerDeserializer::Deserialize(MutableString& text) {
  return mDeserializer.Deserialize(text);
}

inline bool LayerDeserializer::Deserialize(ResizableString& text) {
  return mDeserializer.Deserialize(text);
}

inline bool LayerDeserializer::DeserializeData(unsigned char* data, size_t size) {
  return mDeserializer.DeserializeData(data, size);
}

inline bool LayerDeserializer::ReferenceObject(rtti::castable_rawptr_t object) {
  return mDeserializer.ReferenceObject(object);
}

inline bool LayerDeserializer::BeginCommand(Command& command) {
  const Command* previous_command = mCurrentCommand;

  command.PreviousCommand() = previous_command;

  if (mDeserializer.BeginCommand(command)) {
    mCurrentCommand = &command;
    OrkAssert(command.PreviousCommand() == previous_command);
    return true;
  }

  return false;
}

inline bool LayerDeserializer::EndCommand(const Command& command) {
  OrkAssert(mCurrentCommand == &command);

  mCurrentCommand = mCurrentCommand->PreviousCommand();

  return mDeserializer.EndCommand(command);
}

}}} // namespace ork::reflect::serialize
