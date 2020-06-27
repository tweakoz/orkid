////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/Command.h>
#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serialize {

struct LayerSerializer : public ISerializer {

  LayerSerializer(ISerializer& serializer);

  void Hint(const PieceString&, hintvar_t val) override;

  void serializeData(const uint8_t*, size_t) override;
  void serializeItem(const hintvar_t&) override;
  void serializeObjectProperty(const ObjectProperty*, object_constptr_t) override;
  void serializeSharedObject(object_constptr_t) override;

  void beginCommand(const Command&) override;
  void endCommand(const Command&) override;

protected:
  ISerializer& mSerializer;
};

inline LayerSerializer::LayerSerializer(ISerializer& serializer)
    : mSerializer(serializer) {
}

inline void LayerSerializer::serializeItem(const hintvar_t& value) {
  mSerializer.serializeItem(value);
}

inline void LayerSerializer::Hint(const PieceString& hint, hintvar_t val) {
  mSerializer.Hint(hint, val);
}

inline void LayerSerializer::serializeData(const uint8_t* data, size_t size) {
  mSerializer.serializeData(data, size);
}

inline void LayerSerializer::serializeSharedObject(object_constptr_t object) {
  mSerializer.serializeSharedObject(object);
}

inline void LayerSerializer::serializeObjectProperty(const ObjectProperty* prop, object_constptr_t object) {
  prop->serialize(*this, object);
}

inline void LayerSerializer::beginCommand(const Command& command) {
  const Command* previous_command = _currentCommand;
  command.PreviousCommand()       = previous_command;
  mSerializer.beginCommand(command);
  _currentCommand = &command;
  OrkAssert(command.PreviousCommand() == previous_command);
}

inline void LayerSerializer::endCommand(const Command& command) {
  if (&command == _currentCommand) {
    _currentCommand = _currentCommand->PreviousCommand();
  }
  mSerializer.endCommand(command);
}

}}} // namespace ork::reflect::serialize
