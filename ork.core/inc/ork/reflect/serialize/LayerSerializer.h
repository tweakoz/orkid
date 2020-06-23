////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/Command.h>
#include <ork/rtti/Category.h>

namespace ork { namespace stream {
class IOutputStream;
}} // namespace ork::stream

namespace ork { namespace reflect { namespace serialize {

class LayerSerializer : public ISerializer {
public:
  LayerSerializer(ISerializer& serializer);

  bool Serialize(const bool&) override;
  bool Serialize(const char&) override;
  bool Serialize(const short&) override;
  bool Serialize(const int&) override;
  bool Serialize(const long&) override;
  bool Serialize(const float&) override;
  bool Serialize(const double&) override;
  bool Serialize(const PieceString&) override;
  void Hint(const PieceString&) override;
  void Hint(const PieceString&, intptr_t ival) override;

  bool SerializeData(unsigned char*, size_t) override;

  bool Serialize(const IProperty*) override;
  bool serializeObject(const rtti::ICastable*) override;
  bool serializeObjectProperty(const IObjectProperty*, const Object*) override;
  // bool serializeObjectWithCategory(
  //  const rtti::Category*, //
  // const rtti::ICastable*) override;

  bool ReferenceObject(const rtti::ICastable*) override;
  bool BeginCommand(const Command&) override;
  bool EndCommand(const Command&) override;

protected:
  ISerializer& mSerializer;
  const Command* mCurrentCommand;
};

inline LayerSerializer::LayerSerializer(ISerializer& serializer)
    : mSerializer(serializer)
    , mCurrentCommand(NULL) {
}

inline bool LayerSerializer::Serialize(const bool& value) {
  return mSerializer.Serialize(value);
}

inline bool LayerSerializer::Serialize(const char& value) {
  return mSerializer.Serialize(value);
}

inline bool LayerSerializer::Serialize(const short& value) {
  return mSerializer.Serialize(value);
}

inline bool LayerSerializer::Serialize(const int& value) {
  return mSerializer.Serialize(value);
}

inline bool LayerSerializer::Serialize(const long& value) {
  return mSerializer.Serialize(value);
}

inline bool LayerSerializer::Serialize(const float& value) {
  return mSerializer.Serialize(value);
}

inline bool LayerSerializer::Serialize(const double& value) {
  return mSerializer.Serialize(value);
}

inline bool LayerSerializer::Serialize(const PieceString& text) {
  return mSerializer.Serialize(text);
}

inline void LayerSerializer::Hint(const PieceString& hint) {
  mSerializer.Hint(hint);
}
inline void LayerSerializer::Hint(const PieceString& hint, intptr_t ival) {
  mSerializer.Hint(hint, ival);
}

inline bool LayerSerializer::SerializeData(unsigned char* data, size_t size) {
  return mSerializer.SerializeData(data, size);
}

inline bool LayerSerializer::Serialize(const IProperty* prop) {
  return prop->Serialize(*this);
}

inline bool LayerSerializer::serializeObject(const rtti::ICastable* object) {
  return mSerializer.Serialize(object);
}

inline bool LayerSerializer::serializeObjectProperty(const IObjectProperty* prop, const Object* object) {
  return prop->Serialize(*this, object);
}

// inline bool LayerSerializer::serializeObjectWithCategory(const rtti::Category* category, const rtti::ICastable* object) {
// return category->SerializeReference(*this, object);
//}

inline bool LayerSerializer::ReferenceObject(const rtti::ICastable* object) {
  return mSerializer.ReferenceObject(object);
}

inline bool LayerSerializer::BeginCommand(const Command& command) {
  const Command* previous_command = mCurrentCommand;

  command.PreviousCommand() = previous_command;

  if (mSerializer.BeginCommand(command)) {
    mCurrentCommand = &command;
    OrkAssert(command.PreviousCommand() == previous_command);
    return true;
  }

  return false;
}

inline bool LayerSerializer::EndCommand(const Command& command) {
  if (&command == mCurrentCommand) {
    mCurrentCommand = mCurrentCommand->PreviousCommand();
  }

  return mSerializer.EndCommand(command);
}

}}} // namespace ork::reflect::serialize
