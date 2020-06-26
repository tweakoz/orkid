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
#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::Object, "Object");

namespace ork {

void Object::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

void Object::xxxSerializeShared(
    object_constptr_t obj, //
    reflect::ISerializer& serializer) {
  rtti::Class* clazz = obj->GetClass();
  reflect::Command command(reflect::Command::EOBJECT, clazz->Name());
  serializer.beginCommand(command);
  serializer.referenceObject(obj); // probably wrong..
  obj->PreSerialize(serializer);
  auto objclass    = dynamic_cast<object::ObjectClass*>(clazz);
  const auto& desc = objclass->Description();
  desc.serializeProperties(serializer, obj);
  obj->PostSerialize(serializer);
  serializer.endCommand(command);
}

///////////////////////////////////////////////////////////////////////////////

void Object::xxxDeserializeShared(
    object_ptr_t obj, //
    reflect::IDeserializer& deserializer) {
  rtti::Class* clazz = obj->GetClass();
  deserializer.referenceObject(obj); // probably wrong...
  obj->PreDeserialize(deserializer);
  auto objclass    = dynamic_cast<object::ObjectClass*>(clazz);
  const auto& desc = objclass->Description();
  desc.deserializeProperties(deserializer, obj);
  obj->PostDeserialize(deserializer);
}

///////////////////////////////////////////////////////////////////////////////

object::Signal* Object::FindSignal(ConstString name) {
  auto objclazz = rtti::downcast<object::ObjectClass*>(GetClass());
  auto pSignal  = objclazz->Description().FindSignal(name);

  if (pSignal != 0)
    return &(this->*pSignal);
  else
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::PreSerialize(reflect::ISerializer&) const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::PreDeserialize(reflect::IDeserializer&) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::PostSerialize(reflect::ISerializer&) const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::PostDeserialize(reflect::IDeserializer&) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

/*Object* Object::Clone() const {
  auto the_clone = dynamic_cast<Object*>(GetClass()->CreateObject());
  _cloneInto(the_clone);
  return the_clone;
}*/

///////////////////////////////////////////////////////////////////////////////

object_ptr_t Object::cloneShared() const {
  auto the_clone = GetClass()->createShared();
  _cloneInto(the_clone);
  return the_clone;
}

///////////////////////////////////////////////////////////////////////////////

void Object::_cloneInto(object_ptr_t& into) const {
  /*printf("slowclone class<%s>\n", GetClass()->Name().c_str());

  ork::ResizableString str;
  ork::stream::ResizableStringOutputStream ostream(str);
  ork::reflect::serialize::BinarySerializer binoser(ostream);
  ork::reflect::serialize::ShallowSerializer oser(binoser);

  GetClass()->Description().serializeProperties(oser, this);

  ork::stream::StringInputStream istream(str);
  ork::reflect::serialize::BinaryDeserializer biniser(istream);
  ork::reflect::serialize::ShallowDeserializer iser(biniser);

  GetClass()->Description().DeserializeProperties(iser, into);*/
}

///////////////////////////////////////////////////////////////////////////////

Md5Sum Object::CalcMd5() const {
  // ork::ResizableString str;
  // ork::stream::ResizableStringOutputStream ostream(str);
  // ork::reflect::serialize::BinarySerializer binoser(ostream);
  // ork::reflect::serialize::ShallowSerializer oser(binoser);
  // GetClass()->Description().SerializeProperties(binoser, this);

  CMD5 md5_context;
  // md5_context.update((const uint8_t*)str.data(), str.length());
  // md5_context.finalize();
  return md5_context.Result();
}

///////////////////////////////////////////////////////////////////////////////

reflect::BidirectionalSerializer& operator||(
    reflect::BidirectionalSerializer& bidi, //
    object_ptr_t& object) {
  if (bidi.Serializing()) {
    bidi || object_constptr_t(object);
    return bidi;
  } else {
    reflect::IDeserializer& deserializer = *bidi.Deserializer();

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
}

///////////////////////////////////////////////////////////////////////////////

static object_ptr_t LoadObjectFromFile(
    ConstString filename, //
    bool binary) {
  float ftime1 = ork::OldSchool::GetRef().GetLoResRelTime();
  stream::FileInputStream stream(filename.c_str());

  object_ptr_t object = nullptr;
  if (binary) {
    reflect::serialize::BinaryDeserializer deserializer(stream);

    DeserializeUnknownObject(deserializer, object);
  } else {
    reflect::serialize::JsonDeserializer deserializer(stream);

    DeserializeUnknownObject(deserializer, object);
  }

  float ftime2 = ork::OldSchool::GetRef().GetLoResRelTime();

  static float ftotaltime = 0.0f;
  static int iltotaltime  = 0;

  ftotaltime += (ftime2 - ftime1);

  int itotaltime = int(ftotaltime);

  // if( itotaltime > iltotaltime )
  {
    std::string outstr = ork::CreateFormattedString("MOX AccumTime<%f>\n", ftotaltime);
    // OutputDebugString( outstr.c_str() );
    iltotaltime = itotaltime;
  }

  return object;
}

///////////////////////////////////////////////////////////////////////////////

object_ptr_t DeserializeObject(PieceString file) {
  ArrayString<256> filename_data = file;
  MutableString filename(filename_data);

  if (filename.substr(filename.length() - 4) == ".mox") {
    return LoadObjectFromFile(filename, false);
  } else if (filename.substr(filename.length() - 4) == ".mob") {
    return LoadObjectFromFile(filename, true);
  } else {
    filename = file;
    filename += ".mox";

    if (FileEnv::DoesFileExist(filename.c_str())) {
      return LoadObjectFromFile(filename, false);
    }

    filename = file;
    filename += ".mob";

    if (FileEnv::DoesFileExist(filename.c_str())) {
      return LoadObjectFromFile(filename, true);
    }
  }

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork
