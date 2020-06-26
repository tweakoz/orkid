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
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
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

bool Object::xxxSerialize(
    const Object* obj, //
    reflect::ISerializer& serializer) {
  rtti::Class* clazz = obj->GetClass();
  reflect::Command command(reflect::Command::EOBJECT, clazz->Name());
  serializer.beginCommand(command);
  serializer.ReferenceObject(obj);
  obj->PreSerialize(serializer);
  auto objclass    = dynamic_cast<object::ObjectClass*>(clazz);
  const auto& desc = objclass->Description();
  if (not desc.SerializeProperties(serializer, obj)) {
    OrkAssert(false);
  }
  obj->PostSerialize(serializer);
  serializer.endCommand(command);

  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::xxxSerializeShared(
    object_constptr_t obj, //
    reflect::ISerializer& serializer) {
  rtti::Class* clazz = obj->GetClass();
  reflect::Command command(reflect::Command::EOBJECT, clazz->Name());
  serializer.beginCommand(command);
  serializer.ReferenceObject(obj.get()); // probably wrong..
  obj->PreSerialize(serializer);
  auto objclass    = dynamic_cast<object::ObjectClass*>(clazz);
  const auto& desc = objclass->Description();
  if (not desc.SerializeProperties(serializer, obj.get())) {
    OrkAssert(false);
  }
  obj->PostSerialize(serializer);
  serializer.endCommand(command);

  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::xxxSerializeInPlace(
    const Object* obj, //
    reflect::ISerializer& serializer) {
  rtti::Class* clazz = obj->GetClass();
  reflect::Command command(reflect::Command::EOBJECT, clazz->Name());
  serializer.beginCommand(command);
  obj->PreSerialize(serializer);
  auto objclass    = dynamic_cast<object::ObjectClass*>(clazz);
  const auto& desc = objclass->Description();
  if (not desc.SerializeProperties(serializer, obj)) {
    OrkAssert(false);
  }
  obj->PostSerialize(serializer);
  serializer.endCommand(command);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::xxxDeserialize(
    Object* obj, //
    reflect::IDeserializer& deserializer) {
  rtti::Class* clazz = obj->GetClass();
  deserializer.ReferenceObject(obj);
  obj->PreDeserialize(deserializer);
  auto objclass    = dynamic_cast<object::ObjectClass*>(clazz);
  const auto& desc = objclass->Description();
  if (not desc.DeserializeProperties(deserializer, obj)) {
    OrkAssert(false);
  }
  obj->PostDeserialize(deserializer);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::xxxDeserializeShared(
    object_ptr_t obj, //
    reflect::IDeserializer& deserializer) {
  rtti::Class* clazz = obj->GetClass();
  deserializer.ReferenceObject(obj.get()); // probably wrong...
  obj->PreDeserialize(deserializer);
  auto objclass    = dynamic_cast<object::ObjectClass*>(clazz);
  const auto& desc = objclass->Description();
  if (not desc.DeserializeProperties(deserializer, obj.get())) {
    OrkAssert(false);
  }
  obj->PostDeserialize(deserializer);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Object::xxxDeserializeInPlace(
    Object* obj, //
    reflect::IDeserializer& deserializer) {
  rtti::Class* clazz = obj->GetClass();
  reflect::Command command(reflect::Command::EOBJECT, clazz->Name());
  deserializer.beginCommand(command);
  obj->PreDeserialize(deserializer);
  if (not dynamic_cast<object::ObjectClass*>(clazz)->Description().DeserializeProperties(deserializer, obj)) {
    OrkAssert(false);
  }
  obj->PostDeserialize(deserializer);
  deserializer.endCommand(command);
  return true;
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

Object* Object::Clone() const {
  auto the_clone = dynamic_cast<Object*>(GetClass()->CreateObject());
  _cloneInto(the_clone);
  return the_clone;
}

///////////////////////////////////////////////////////////////////////////////

object_ptr_t Object::cloneShared() const {
  auto the_clone = std::dynamic_pointer_cast<Object>(GetClass()->createShared());
  _cloneInto(the_clone.get());
  return the_clone;
}

///////////////////////////////////////////////////////////////////////////////

void Object::_cloneInto(Object* into) const {
  printf("slowclone class<%s>\n", GetClass()->Name().c_str());

  ork::ResizableString str;
  ork::stream::ResizableStringOutputStream ostream(str);
  ork::reflect::serialize::BinarySerializer binoser(ostream);
  ork::reflect::serialize::ShallowSerializer oser(binoser);

  GetClass()->Description().SerializeProperties(oser, this);

  ork::stream::StringInputStream istream(str);
  ork::reflect::serialize::BinaryDeserializer biniser(istream);
  ork::reflect::serialize::ShallowDeserializer iser(biniser);

  GetClass()->Description().DeserializeProperties(iser, into);
}

///////////////////////////////////////////////////////////////////////////////

Md5Sum Object::CalcMd5() const {
  ork::ResizableString str;
  ork::stream::ResizableStringOutputStream ostream(str);
  ork::reflect::serialize::BinarySerializer binoser(ostream);
  // ork::reflect::serialize::ShallowSerializer oser(binoser);
  GetClass()->Description().SerializeProperties(binoser, this);

  CMD5 md5_context;
  md5_context.update((const uint8_t*)str.data(), str.length());
  md5_context.finalize();

  return md5_context.Result();
}

///////////////////////////////////////////////////////////////////////////////

reflect::BidirectionalSerializer& operator||(
    reflect::BidirectionalSerializer& bidi, //
    Object& object) {
  if (bidi.Serializing()) {
    return bidi || static_cast<const Object&>(object);
  } else {
    reflect::IDeserializer& deserializer = *bidi.Deserializer();

    reflect::Command object_command;

    if (false == deserializer.beginCommand(object_command))
      bidi.Fail();

    rtti::Class* clazz = rtti::Class::FindClass(object_command.Name());

    OrkAssertI(object.GetClass()->IsSubclassOf(clazz), "Can't deserialize an X into a Y");

    if (object.GetClass()->IsSubclassOf(clazz)) {
      if (false == Object::xxxDeserialize(&object, deserializer))
        bidi.Fail();
    }

    if (false == deserializer.endCommand(object_command))
      bidi.Fail();
  }

  return bidi;
}

///////////////////////////////////////////////////////////////////////////////

reflect::BidirectionalSerializer& operator||(
    reflect::BidirectionalSerializer& bidi, //
    const Object& object) {
  OrkAssertI(bidi.Serializing(), "can't deserialize to a non-const object");

  if (bidi.Serializing()) {
    if (false == Object::xxxSerialize(&object, *bidi.Serializer()))
      bidi.Fail();
  }

  return bidi;
}

///////////////////////////////////////////////////////////////////////////////

static Object* LoadObjectFromFile(
    ConstString filename, //
    bool binary) {
  float ftime1 = ork::OldSchool::GetRef().GetLoResRelTime();
  stream::FileInputStream stream(filename.c_str());

  Object* object = nullptr;
  if (binary) {
    reflect::serialize::BinaryDeserializer deserializer(stream);

    DeserializeUnknownObject(deserializer, object);
  } else {
    reflect::serialize::XMLDeserializer deserializer(stream);

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

Object* DeserializeObject(PieceString file) {
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
