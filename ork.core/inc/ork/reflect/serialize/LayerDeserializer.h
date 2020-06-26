////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/properties/AbstractProperty.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/Command.h>

#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serialize {

class LayerDeserializer : public IDeserializer {
public:
  LayerDeserializer(IDeserializer& deserializer);

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

  void deserialize(const AbstractProperty*) override;
  bool deserializeObject(rtti::castable_rawptr_t&) override;
  bool deserializeSharedObject(rtti::castable_ptr_t&) override;
  bool deserializeObjectProperty(const ObjectProperty*, Object*) override;

  void referenceObject(rtti::castable_rawptr_t) override;
  void beginCommand(Command&) override;
  void endCommand(const Command&) override;

protected:
  bool _deserialize(const rtti::Category*, rtti::castable_rawptr_t&);

protected:
  IDeserializer& mDeserializer;
};

inline LayerDeserializer::LayerDeserializer(IDeserializer& deserializer)
    : mDeserializer(deserializer) {
}

inline void LayerDeserializer::deserialize(bool& value) {
  mDeserializer.Deserialize(value);
}

inline void LayerDeserializer::deserialize(char& value) {
  mDeserializer.Deserialize(value);
}

inline void LayerDeserializer::deserialize(short& value) {
  mDeserializer.Deserialize(value);
}

inline void LayerDeserializer::deserialize(int& value) {
  mDeserializer.Deserialize(value);
}

inline void LayerDeserializer::deserialize(long& value) {
  mDeserializer.Deserialize(value);
}

inline void LayerDeserializer::deserialize(float& value) {
  mDeserializer.Deserialize(value);
}

inline void LayerDeserializer::deserialize(double& value) {
  mDeserializer.Deserialize(value);
}

inline void LayerDeserializer::deserializeObject(rtti::castable_rawptr_t& value) {
  mDeserializer.deserializeObject(value);
}
inline void LayerDeserializer::deserializeSharedObject(rtti::castable_ptr_t& value) {
  mDeserializer.deserializeSharedObject(value);
}

inline void LayerDeserializer::deserialize(const AbstractProperty* prop) {
  prop->Deserialize(*this);
}

inline void LayerDeserializer::deserializeObjectProperty(const ObjectProperty* prop, Object* object) {
  prop->deserialize(*this, object);
}

inline void LayerDeserializer::_deserialize(const rtti::Category* category, rtti::castable_rawptr_t& object) {
  category->deserializeObject(*this, object);
}

inline void LayerDeserializer::deserialize(MutableString& text) {
  mDeserializer.Deserialize(text);
}

inline void LayerDeserializer::deserialize(ResizableString& text) {
  mDeserializer.Deserialize(text);
}

inline void LayerDeserializer::deserializeData(unsigned char* data, size_t size) {
  mDeserializer.DeserializeData(data, size);
}

inline void LayerDeserializer::ReferenceObject(rtti::castable_rawptr_t object) {
  mDeserializer.ReferenceObject(object);
}

inline void LayerDeserializer::beginCommand(Command& command) {
  const Command* previous_command = _currentCommand;
  command.PreviousCommand()       = previous_command;
  mDeserializer.beginCommand(command);
  _currentCommand = &command;
  OrkAssert(command.PreviousCommand() == previous_command);
}

inline void LayerDeserializer::endCommand(const Command& command) {
  OrkAssert(_currentCommand == &command);
  _currentCommand = _currentCommand->PreviousCommand();
  mDeserializer.endCommand(command);
}

}}} // namespace ork::reflect::serialize
