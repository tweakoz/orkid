////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/Serialize.h>

namespace ork::reflect::serdes {

template <typename T>
inline //
    BidirectionalSerializer&
    BidirectionalSerializer::operator|(T& object) {
  //::ork::reflect::Serialize<T>(&object, &object, *this);
  return *this;
}

template <typename T>
inline //
    BidirectionalSerializer&
    BidirectionalSerializer::operator|(const T& object) {
  //::ork::reflect::Serialize<T>(&object, NULL, *this);
  return *this;
}

template <typename ClassType>
inline //
    BidirectionalSerializer::Stream<ClassType>
    BidirectionalSerializer::stream(
        const ClassType* in, //
        ClassType* out) {
  return Stream<ClassType>(*this, in, out);
}

template <typename ClassType>
inline BidirectionalSerializer::Stream<ClassType>::Stream(
    BidirectionalSerializer& serializer,
    const ClassType* ser,
    ClassType* deser)
    : mSerializer(serializer)
    , mSerializeObject(ser)
    , mDeserializeObject(deser) {
}

template <typename ClassType> inline BidirectionalSerializer& BidirectionalSerializer::Stream<ClassType>::StreamSerializer() const {
  return mSerializer;
}

template <typename ClassType>
template <typename DataType>
inline                                                        //
    const BidirectionalSerializer::Stream<ClassType>&         //
        BidirectionalSerializer::Stream<ClassType>::operator| //
    (DataType ClassType::*member) const {
  if (mSerializer.Serializing()) {
    //::ork::reflect::Serialize<DataType>(&(mSerializeObject->*member), NULL, mSerializer);
  } else {
    //::ork::reflect::Serialize<DataType>(NULL, &(mDeserializeObject->*member), mSerializer);
  }

  return *this;
}
} //namespace ork::reflect::serdes {
