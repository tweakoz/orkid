////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/orkconfig.h>

#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/file/path.h>
#include <ork/object/Object.h>

namespace ork { namespace rtti {
class ICastable;
}} // namespace ork::rtti

namespace ork { namespace reflect {

//////////////////////////////////////////////////
void BidirectionalSerializer::serializeSharedObject(object_constptr_t obj) {
  // printf("bidi sershared<%p>\n", obj.get());
  mSerializer->serializeSharedObject(obj);
}
void BidirectionalSerializer::deserializeSharedObject(object_ptr_t& obj) {
  mDeserializer->deserializeSharedObject(obj);
}

//////////////////////////////////////////////////
template <typename T>
inline //
    void
    BidirectionalSerializer::Serialize(const T& value) {
  mSerializer->serialize(value);
}

template <typename T>
inline //
    void
    BidirectionalSerializer::Deserialize(T& value) {
  mDeserializer->deserialize(value);
}

//////////////////////////////////////////////////

template <typename T>
void Serialize(  // default serdes<>bidi handler
    const T* in, //
    T* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serialize(*in);
  } else {
    bidi.Deserialize(*out);
  }
}

//////////////////////////////////////////////////

#define FOREACH_BASIC_SERIALIZATION_TYPE(MACRO)                                                                                    \
  MACRO(bool);                                                                                                                     \
  MACRO(char);                                                                                                                     \
  MACRO(short);                                                                                                                    \
  MACRO(int);                                                                                                                      \
  MACRO(long);                                                                                                                     \
  MACRO(float);                                                                                                                    \
  MACRO(double);

#define INSTANTIATE_SERIALIZE_FUNCTION(Type) template void Serialize<Type>(const Type*, Type*, BidirectionalSerializer&);

FOREACH_BASIC_SERIALIZATION_TYPE(INSTANTIATE_SERIALIZE_FUNCTION)

//////////////////////////////////////////////////

template <>
void Serialize(
    object_ptr_t const* in, //
    object_ptr_t* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.serializeSharedObject(*in);
  } else {
    object_ptr_t ptr;
    bidi.deserializeSharedObject(ptr);
    (*out) = ptr;
  }
}

template <>
void Serialize(
    const std::string* in, //
    std::string* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serialize(ork::PieceString(in->c_str()));
  } else {
    ResizableString result;
    bidi.Deserialize(result);
    *out = result.c_str();
  }
}
template <>
void Serialize(
    const file::Path* in, //
    file::Path* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serialize(ork::PieceString(in->c_str()));
  } else {
    ResizableString result;
    bidi.Deserialize(result);
    *out = result.c_str();
  }
}

template <>
void Serialize(
    const Char4* in, //
    Char4* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serialize(ork::PieceString(in->c_str()));
  } else {
    ResizableString result;
    bidi.Deserialize(result);
    OrkAssert(result.length() <= 4);
    out->SetCString(result.c_str());
  }
}
template <>
void Serialize(
    const Char8* in, //
    Char8* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serialize(ork::PieceString(in->c_str()));
  } else {
    ResizableString result;
    bidi.Deserialize(result);
    OrkAssert(result.length() <= 8);
    out->SetCString(result.c_str());
  }
}

}} // namespace ork::reflect
