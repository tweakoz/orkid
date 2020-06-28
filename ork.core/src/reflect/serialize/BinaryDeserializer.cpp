////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/reflect/Command.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/downcast.h>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>

namespace ork::reflect::serialize {
////////////////////////////////////////////////////////////////////////////////
BinaryDeserializer::BinaryDeserializer(stream::IInputStream& stream)
    : mStream(stream) {
}
////////////////////////////////////////////////////////////////////////////////
BinaryDeserializer::~BinaryDeserializer() {
  for (int i = 0; i < mStringPool.Size(); i++)
    delete mStringPool.FromIndex(i).c_str();
}
////////////////////////////////////////////////////////////////////////////////
template <> //
void BinaryDeserializer::Read<ConstString>(ConstString& text) {
  int len_or_backref;

  Read(len_or_backref);

  if (len_or_backref < 0) {
    int len = -(len_or_backref + 1);

    char* data = new char[len + 1];
    mStream.Read(reinterpret_cast<unsigned char*>(data), size_t(len));
    data[len] = '\0';

    text = data;

    mStringPool.Literal(data);
    return;
  }
  text = mStringPool.FromIndex(len_or_backref).c_str();
}
////////////////////////////////////////////////////////////////////////////////
template <typename T> void BinaryDeserializer::Read(T& value) {
  bool ok = mStream.Read(reinterpret_cast<unsigned char*>(&value), sizeof(T)) == sizeof(T);
  OrkAssert(ok);
}
////////////////////////////////////////////////////////////////////////////////
template void BinaryDeserializer::Read<char>(char&);
template void BinaryDeserializer::Read<short>(short&);
template void BinaryDeserializer::Read<int>(int&);
template void BinaryDeserializer::Read<long>(long&);
template void BinaryDeserializer::Read<float>(float&);
template void BinaryDeserializer::Read<double>(double&);
template void BinaryDeserializer::Read<bool>(bool&);
////////////////////////////////////////////////////////////////////////////////
void BinaryDeserializer::deserializeObjectProperty(
    const ObjectProperty* prop, //
    object_ptr_t instance) {
  prop->deserialize(*this, instance);
}
////////////////////////////////////////////////////////////////////////////////
char BinaryDeserializer::Peek() {
  char c;
  if (mStream.Peek(reinterpret_cast<unsigned char*>(&c), 1) == 1) {
    return c;
  } else {
    return 0;
  }
}
////////////////////////////////////////////////////////////////////////////////
bool BinaryDeserializer::Match(char c) {
  if (Peek() == c) {
    Read(c);
    return true;
  } else {
    return false;
  }
}
////////////////////////////////////////////////////////////////////////////////
void BinaryDeserializer::deserializeSharedObject(object_ptr_t& instance_out) {
  std::string object_uuid_str;
  if (Match('N')) {
    instance_out = nullptr;
  }
  if (Match('B')) {
    Read(object_uuid_str);
    boost::uuids::string_generator gen;
    auto as_uuid = gen(object_uuid_str);
    instance_out = findTrackedObject(as_uuid); //
  } else if (Match('R')) {
    std::string object_uuid_str;
    Read(object_uuid_str);
    ConstString classname;
    Read(classname);
    auto category = rtti::downcast<rtti::Category*>(rtti::Class::FindClass(classname));
    boost::uuids::string_generator gen;
    auto as_uuid = gen(object_uuid_str);
    category->deserializeObject(*this, instance_out);
    trackObject(as_uuid, instance_out);
    if (false == Match('r')) {
      OrkAssert(false);
    }
  } else {
    OrkAssert(false);
  }
}
////////////////////////////////////////////////////////////////////////////////
void BinaryDeserializer::beginCommand(Command& command) {
  if (Match('O')) {
    ConstString name;
    Read(name);
    command.Setup(Command::EOBJECT, name);
  } else if (Match('P')) {
    ConstString name;
    Read(name);
    command.Setup(Command::EPROPERTY, name);
  } else if (Match('A')) {
    ConstString name;
    Read(name);
    command.Setup(Command::EATTRIBUTE, name);
  } else if (Match('I')) {
    command.Setup(Command::EITEM, "");
  }
  command.PreviousCommand() = _currentCommand;
  _currentCommand           = &command;
}
////////////////////////////////////////////////////////////////////////////////
void BinaryDeserializer::endCommand(const Command& command) {
  OrkAssert(_currentCommand == &command);
  bool matched = false;
  switch (command.Type()) {
    case Command::EPROPERTY:
      matched = Match('p');
      break;
    case Command::EATTRIBUTE:
      matched = Match('a');
      break;
    case Command::EOBJECT:
      matched = Match('o');
      break;
    case Command::EITEM:
      matched = Match('i');
      break;
  }
  OrkAssert(matched);
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serialize
