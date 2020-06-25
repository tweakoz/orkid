////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/properties/AbstractProperty.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/stream/IOutputStream.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <ork/object/Object.h>
#include <boost/uuid/uuid_io.hpp>

namespace ork { namespace reflect { namespace serialize {

BinarySerializer::BinarySerializer(stream::IOutputStream& stream)
    : mStream(stream) {
}

BinarySerializer::~BinarySerializer() {
  for (int i = 0; i < mStringPool.Size(); i++)
    delete mStringPool.FromIndex(i).c_str();
}

bool BinarySerializer::WriteHeader(char type, PieceString text) {
  bool result = true;

  result = Write(type);

  return Serialize(text);
}

bool BinarySerializer::WriteFooter(char type) {
  return Write(type);
}

template <typename T> bool BinarySerializer::Write(const T& datum) {
  // Endian issues come up here
  return mStream.Write(
      reinterpret_cast<const unsigned char*>(&datum), //
      sizeof(T));
}

bool BinarySerializer::BeginCommand(const Command& command) {
  switch (command.Type()) {
    case Command::EOBJECT:
      WriteHeader('O', command.Name());
      break;
    case Command::EATTRIBUTE:
      WriteHeader('A', command.Name());
      break;
    case Command::EPROPERTY:
      WriteHeader('P', command.Name());
      break;
    case Command::EITEM:
      Write('I');
      break;
  }
  command.PreviousCommand() = mCurrentCommand;
  mCurrentCommand           = &command;
  return true;
}

bool BinarySerializer::EndCommand(const Command& command) {

  if (mCurrentCommand == &command) {
    mCurrentCommand = mCurrentCommand->PreviousCommand();
  } else {
    orkprintf(
        "Mismatched Serializer commands! expected: %s got: %s\n",
        mCurrentCommand ? mCurrentCommand->Name().c_str() : "<no command>",
        command.Name().c_str());
  }

  switch (command.Type()) {
    case Command::EOBJECT:
      WriteFooter('o');
      break;
    case Command::EATTRIBUTE:
      WriteFooter('a');
      break;
    case Command::EPROPERTY:
      WriteFooter('p');
      break;
    case Command::EITEM:
      WriteFooter('i');
      break;
  }

  return true;
}

bool BinarySerializer::Serialize(const char& value) {
  return Write(value);
}

bool BinarySerializer::Serialize(const short& value) {
  return Write(value);
}

bool BinarySerializer::Serialize(const int& value) {
  return Write(value);
}

bool BinarySerializer::Serialize(const long& value) {
  return Write(value);
}

bool BinarySerializer::Serialize(const float& value) {
  return Write(value);
}

bool BinarySerializer::Serialize(const double& value) {
  return Write(value);
}

bool BinarySerializer::Serialize(const bool& value) {
  return Write(value);
}

bool BinarySerializer::Serialize(const AbstractProperty* prop) {
  return prop->Serialize(*this);
}

bool BinarySerializer::serializeObjectProperty(const ObjectProperty* prop, const Object* object) {
  return prop->Serialize(*this, object);
}

bool BinarySerializer::ReferenceObject(const rtti::ICastable* castable) {
  auto as_object    = dynamic_cast<const ork::Object*>(castable);
  const auto& uuid  = as_object->_uuid;
  std::string uuids = boost::uuids::to_string(uuid);
  OrkAssert(_serialized.find(uuids) == _serialized.end());
  _serialized.insert(uuids);
  return true;
}

bool BinarySerializer::serializeObject(const rtti::ICastable* castable) {
  auto as_object = dynamic_cast<const ork::Object*>(castable);
  if (as_object == nullptr) {
    Write('N');
  } else {
    const auto& uuid  = as_object->_uuid;
    std::string uuids = boost::uuids::to_string(uuid);

    auto it = _serialized.find(uuids);

    ////////////////////////////////////
    // backreference
    ////////////////////////////////////
    if (it != _serialized.end()) {
      Write('B');
      Write(uuids.c_str());
    }
    ////////////////////////////////////
    // firstreference
    ////////////////////////////////////
    else {
      auto objclazz = as_object->GetClass();
      auto category = rtti::downcast<rtti::Category*>(objclazz->GetClass());
      WriteHeader('R', category->Name());
      Write(uuids.c_str());
      category->serializeObject(*this, as_object);
      WriteFooter('r');
    }
  }

  return true;
}

void BinarySerializer::Hint(const PieceString&) {
}
void BinarySerializer::Hint(const PieceString&, intptr_t ival) {
}

bool BinarySerializer::Serialize(const PieceString& text) {
  bool result = true;

  int pooled_string_index = mStringPool.FindIndex(text);

  if (pooled_string_index == -1) {
    result = Write(-int(text.length() + 1));
    mStream.Write(reinterpret_cast<const unsigned char*>(text.c_str()), text.length());

    char* text_copy = new char[text.length() + 1];
    memcpy(text_copy, text.c_str(), text.length());
    text_copy[text.length()] = '\0';
    mStringPool.Literal(text_copy);
  } else {
    result = Write(int(pooled_string_index));
  }

  return result;
}

bool BinarySerializer::SerializeData(unsigned char* data, size_t size) {
  bool result = mStream.Write(data, size);
  return result;
}

}}} // namespace ork::reflect::serialize
