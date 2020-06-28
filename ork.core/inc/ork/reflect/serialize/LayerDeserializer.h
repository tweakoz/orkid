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

namespace ork { namespace reflect { namespace serialize {

class LayerDeserializer : public IDeserializer {
public:
  LayerDeserializer(IDeserializer& deserializer);

  void deserializeSharedObject(object_ptr_t&) override;
  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;

  void beginCommand(Command&) override;
  void endCommand(const Command&) override;
  void deserializeItem() override;

protected:
  void _deserialize(const rtti::Category*, object_ptr_t&);

protected:
  IDeserializer& mDeserializer;
};

inline LayerDeserializer::LayerDeserializer(IDeserializer& deserializer)
    : mDeserializer(deserializer) {
}

inline void LayerDeserializer::deserializeItem() {
  mDeserializer.deserializeItem();
}

inline void LayerDeserializer::deserializeSharedObject(object_ptr_t& value) {
  mDeserializer.deserializeSharedObject(value);
}

inline void LayerDeserializer::deserializeObjectProperty(const ObjectProperty* prop, object_ptr_t object) {
  prop->deserialize(*this, object);
}

inline void LayerDeserializer::_deserialize(const rtti::Category* category, object_ptr_t& object) {
  category->deserializeObject(*this, object);
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
