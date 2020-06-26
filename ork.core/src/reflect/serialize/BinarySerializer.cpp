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

namespace ork::reflect::serialize {
////////////////////////////////////////////////////////////////////////////////
BinarySerializer::BinarySerializer(stream::IOutputStream& stream)
    : mStream(stream) {
}
////////////////////////////////////////////////////////////////////////////////
BinarySerializer::~BinarySerializer() {
  for (int i = 0; i < mStringPool.Size(); i++)
    delete mStringPool.FromIndex(i).c_str();
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::WriteHeader(char type, PieceString text) {
  Write(type);
  Serialize(text);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::WriteFooter(char type) {
  Write(type);
}
////////////////////////////////////////////////////////////////////////////////
template <typename T> void BinarySerializer::Write(const T& datum) {
  // Endian issues come up here
  bool ok = mStream.Write(
      reinterpret_cast<const unsigned char*>(&datum), //
      sizeof(T));
  OrkAssert(ok);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::beginCommand(const Command& command) {
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
  command.PreviousCommand() = _currentCommand;
  _currentCommand           = &command;
  return true;
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::endCommand(const Command& command) {
  if (_currentCommand == &command) {
    _currentCommand = _currentCommand->PreviousCommand();
  } else {
    orkprintf(
        "Mismatched Serializer commands! expected: %s got: %s\n",
        _currentCommand ? _currentCommand->Name().c_str() : "<no command>",
        command.Name().c_str());
    OrkAssert(false);
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
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const char& value) {
  Write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const short& value) {
  Write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const int& value) {
  Write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const long& value) {
  Write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const float& value) {
  Write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const double& value) {
  Write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const bool& value) {
  Write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const AbstractProperty* prop) {
  prop->Serialize(*this);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::serializeObjectProperty(const ObjectProperty* prop, const Object* object) {
  prop->Serialize(*this, object);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::serializeObject(const rtti::ICastable* castable) {
  auto as_object = dynamic_cast<const ork::Object*>(castable);
  if (as_object == nullptr) {
    Write('N');
  } else {
    const auto& uuid  = as_object->_uuid;
    std::string uuids = boost::uuids::to_string(uuid);
    auto it           = _serialized.find(uuids);
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
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Hint(const PieceString&) {
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Hint(const PieceString&, intptr_t ival) {
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Serialize(const PieceString& text) {
  int pooled_string_index = mStringPool.FindIndex(text);

  if (pooled_string_index == -1) {
    Write(-int(text.length() + 1));
    mStream.Write(reinterpret_cast<const unsigned char*>(text.c_str()), text.length());
    char* text_copy = new char[text.length() + 1];
    memcpy(text_copy, text.c_str(), text.length());
    text_copy[text.length()] = '\0';
    mStringPool.Literal(text_copy);
  } else {
    Write(int(pooled_string_index));
  }
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::SerializeData(unsigned char* data, size_t size) {
  bool ok = mStream.Write(data, size);
  OrkAssert(ok);
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
