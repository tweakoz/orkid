////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/AutoConnector.h>
#include <ork/rtti/Class.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/rtti/downcast.h>

#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::Object, "Object");

namespace ork {

///////////////////////////////////////////////////////////////////////////////
void Object::Describe() {
}
///////////////////////////////////////////////////////////////////////////////
Object::Object() {
  _uuid = object::ObjectClass::genUUID();
}
///////////////////////////////////////////////////////////////////////////////
Object::~Object() {
}
///////////////////////////////////////////////////////////////////////////////
object::ObjectClass* Object::objectClass() const {
  return dynamic_cast<object::ObjectClass*>(GetClass());
}
///////////////////////////////////////////////////////////////////////////////
object::Signal* Object::findSignal(ConstString name) {
  auto objclazz = rtti::downcast<object::ObjectClass*>(GetClass());
  auto pSignal  = objclazz->Description().findSignal(name);

  if (pSignal != 0)
    return &(this->*pSignal);
  else
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void Object::notify(const event::Event* pEV) {
  doNotify(pEV);
}

///////////////////////////////////////////////////////////////////////////////

bool Object::preSerialize(reflect::serdes::ISerializer&) const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::preDeserialize(reflect::serdes::IDeserializer&) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::postSerialize(reflect::serdes::ISerializer&) const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::postDeserialize(reflect::serdes::IDeserializer&) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

object_ptr_t Object::clone(object_constptr_t source) {
  ork::reflect::serdes::JsonSerializer ser;
  ser.serializeRoot(source);
  auto serstream = ser.output();
  ork::reflect::serdes::JsonDeserializer deser(serstream);
  object_ptr_t copy;
  deser.deserializeTop(copy);
  return copy;
}

///////////////////////////////////////////////////////////////////////////////

Md5Sum Object::md5sum(object_constptr_t source) {
  ork::reflect::serdes::JsonSerializer ser;
  auto objnode   = ser.serializeRoot(source);
  auto serstream = ser.output();
  CMD5 md5_context;
  md5_context.update((const uint8_t*)serstream.data(), serstream.length());
  md5_context.finalize();
  return md5_context.Result();
}

///////////////////////////////////////////////////////////////////////////////
/*
reflect::BidirectionalSerializer& operator||(
    reflect::BidirectionalSerializer& bidi, //
    object_ptr_t& object) {
  if (bidi.Serializing()) {
    bidi || object_constptr_t(object);
    return bidi;
  } else {
    reflect::serdes::IDeserializer& deserializer = *bidi.Deserializer();

    reflect::Command object_command;

    deserializer.beginCommand(object_command);
    auto clazz = rtti::Class::FindClass(object_command.Name());
    OrkAssertI(object->GetClass()->IsSubclassOf(clazz), "Can't deserialize an X into a Y");

    if (object->GetClass()->IsSubclassOf(clazz)) {
      Object::xxxDeserializeShared(object, deserializer);
    }

    deserializer.endCommand(object_command);
  }

  return bidi;
}

///////////////////////////////////////////////////////////////////////////////

reflect::BidirectionalSerializer& operator||(
    reflect::BidirectionalSerializer& bidi, //
    object_constptr_t object) {
  OrkAssertI(bidi.Serializing(), "can't deserialize to a non-const object");

  if (bidi.Serializing()) {
    Object::xxxSerializeShared(object, *bidi.Serializer());
  }

  return bidi;
}*/

///////////////////////////////////////////////////////////////////////////////

object_ptr_t loadObjectFromFile(const char* filename) {
  file::Path the_path(filename);
  if (ork::FileEnv::GetRef().DoesFileExist(the_path)) {
    File file(the_path.c_str(), EFM_READ);
    size_t len = 0;
    file.GetLength(len);
    std::string jsondata;
    jsondata.resize(len);
    file.Read((void*)jsondata.c_str(), len);
    return loadObjectFromString(jsondata.c_str());
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

object_ptr_t loadObjectFromString(const char* jsondata) {
  float ftime1 = ork::OldSchool::GetRef().GetLoResRelTime();

  object_ptr_t instance_out = nullptr;
  reflect::serdes::JsonDeserializer deserializer(jsondata);
  deserializer.deserializeTop(instance_out);

  float ftime2 = ork::OldSchool::GetRef().GetLoResRelTime();

  static float ftotaltime = 0.0f;
  static int iltotaltime  = 0;

  ftotaltime += (ftime2 - ftime1);

  int itotaltime = int(ftotaltime);

  // if( itotaltime > iltotaltime )
  {
    std::string outstr = ork::CreateFormattedString("MOJ AccumTime<%f>\n", ftotaltime);
    // OutputDebugString( outstr.c_str() );
    iltotaltime = itotaltime;
  }

  return instance_out;
}

///////////////////////////////////////////////////////////////////////////////
/*
object_ptr_t DeserializeObject(PieceString file) {
  ArrayString<256> filename_data = file;
  MutableString filename(filename_data);

  if (filename.substr(filename.length() - 4) == ".mox") {
    return loadObjectFromFile(filename, false);
  } else if (filename.substr(filename.length() - 4) == ".mob") {
    return loadObjectFromFile(filename, true);
  } else {
    filename = file;
    filename += ".mox";

    if (FileEnv::DoesFileExist(filename.c_str())) {
      return loadObjectFromFile(filename, false);
    }

    filename = file;
    filename += ".mob";

    if (FileEnv::DoesFileExist(filename.c_str())) {
      return loadObjectFromFile(filename, true);
    }
  }

  return nullptr;
}*/

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork
