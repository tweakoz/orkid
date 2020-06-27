////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/reflect/Command.h>

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
void BinarySerializer::_writeHeader(char type, PieceString text) {
  _write(type);
  serializeItem(text);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::_writeFooter(char type) {
  _write(type);
}
////////////////////////////////////////////////////////////////////////////////
template <typename T> void BinarySerializer::_write(const T& datum) {
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
      _writeHeader('O', command.Name());
      break;
    case Command::EATTRIBUTE:
      _writeHeader('A', command.Name());
      break;
    case Command::EPROPERTY:
      _writeHeader('P', command.Name());
      break;
    case Command::EITEM:
      _write('I');
      break;
  }
  command.PreviousCommand() = _currentCommand;
  _currentCommand           = &command;
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
      _writeFooter('o');
      break;
    case Command::EATTRIBUTE:
      _writeFooter('a');
      break;
    case Command::EPROPERTY:
      _writeFooter('p');
      break;
    case Command::EITEM:
      _writeFooter('i');
      break;
  }
}
////////////////////////////////////////////////////////////////////////////////
/*void BinarySerializer::serialize(const PieceString& text) {
  int pooled_string_index = mStringPool.FindIndex(text);

  if (pooled_string_index == -1) {
    _write(-int(text.length() + 1));
    mStream.Write(reinterpret_cast<const unsigned char*>(text.c_str()), text.length());
    char* text_copy = new char[text.length() + 1];
    memcpy(text_copy, text.c_str(), text.length());
    text_copy[text.length()] = '\0';
    mStringPool.Literal(text_copy);
  } else {
    _write(int(pooled_string_index));
  }
}*/
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::serializeItem(const hintvar_t& value) {
  //_write(value);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::serializeObjectProperty(
    const ObjectProperty* prop, //
    object_constptr_t instance) {
  prop->serialize(*this, instance);
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::serializeSharedObject(object_constptr_t instance) {
  if (instance == nullptr) {
    _write('N');
  } else {
    const auto& uuid  = instance->_uuid;
    std::string uuids = boost::uuids::to_string(uuid);
    auto it           = _reftracker.find(uuids);
    ////////////////////////////////////
    // backreference
    ////////////////////////////////////
    if (it != _reftracker.end()) {
      _write('B');
      _write(uuids.c_str());
    }
    ////////////////////////////////////
    // firstreference
    ////////////////////////////////////
    else {
      auto objclazz = instance->GetClass();
      auto category = rtti::downcast<rtti::Category*>(objclazz->GetClass());
      _writeHeader('R', category->Name());
      _write(uuids.c_str());
      category->serializeObject(*this, instance);
      _writeFooter('r');
    }
  }
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::Hint(const PieceString&, hintvar_t val) {
}
////////////////////////////////////////////////////////////////////////////////
void BinarySerializer::serializeData(const uint8_t* data, size_t size) {
  bool ok = mStream.Write(data, size);
  OrkAssert(ok);
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
