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
  return mStream.Write(reinterpret_cast<const unsigned char*>(&datum), sizeof(T));
}

bool BinarySerializer::BeginCommand(const Command& command) {
  bool result = false;

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

  return result;
}

bool BinarySerializer::EndCommand(const Command& command) {
  bool result = false;

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

  return result;
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

bool BinarySerializer::ReferenceObject(const rtti::ICastable* object) {
  OrkAssert(FindObject(object) == -1);

  mSerializedObjects.push_back(object);

  return true;
}

int BinarySerializer::FindObject(const rtti::ICastable* object) {
  int result = -1;

  for (orkvector<const rtti::ICastable*>::size_type index = 0; index < mSerializedObjects.size(); index++) {
    if (mSerializedObjects[index] == object) {
      result = int(index);
      break;
    }
  }

  return result;
}

bool BinarySerializer::serializeObject(const rtti::ICastable* object) {
  bool result = true;

  if (object == NULL) {
    Write('N');
  } else {
    int object_index = FindObject(object);

    if (object_index != -1) {
      Write('B');
      Write(int(object_index));
    } else {
      const rtti::Category* category = rtti::downcast<rtti::Category*>(object->GetClass()->GetClass());

      if (false == WriteHeader('R', category->Name()))
        result = false;

      if (false == category->serializeObject(*this, object))
        result = false;

      if (false == WriteFooter('r'))
        result = false;
    }
  }

  return result;
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
