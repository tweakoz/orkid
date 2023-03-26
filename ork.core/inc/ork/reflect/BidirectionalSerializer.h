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

class BidirectionalSerializer {
public:
  BidirectionalSerializer(IDeserializer&);
  BidirectionalSerializer(ISerializer&);

  template <typename T> BidirectionalSerializer& operator|(T&);

  template <typename T> BidirectionalSerializer& operator|(const T&);

  template <typename ClassType> class Stream {
  public:
    Stream(
        BidirectionalSerializer&, //
        const ClassType*,
        ClassType*);
    template <typename DataType> const Stream& operator|(DataType ClassType::*member) const;

    BidirectionalSerializer& StreamSerializer() const;

  private:
    BidirectionalSerializer& mSerializer;
    const ClassType* mSerializeObject;
    ClassType* mDeserializeObject;
  };

  template <typename ClassType>
  Stream<ClassType> //
  stream(const ClassType*, ClassType*);

  bool Succeeded() const;
  bool Serializing() const;
  void Fail();

  ISerializer* Serializer() const;
  IDeserializer* Deserializer() const;

  // only used in (and defined in) Serialize.cpp
  template <typename T> void Serialize(const T&);

  // only used in (and defined in) Serialize.cpp
  template <typename T> void Deserialize(T&);

  void serializeSharedObject(object_constptr_t);
  void deserializeSharedObject(object_ptr_t&);

  operator bool() const;

private:
  IDeserializer* mDeserializer;
  ISerializer* mSerializer;
  bool mSuccess;
};



} // namespace ork::reflect::serdes
