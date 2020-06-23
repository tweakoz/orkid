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

using ObjectPointer = ork::Object*;

//////////////////////////////////////////////////
template <typename T>
inline //
    void
    BidirectionalSerializer::Serialize(const T& value) {
  bool result = mSerializer->Serialize(value);

  if (false == result)
    Fail();
}

template <typename T>
inline //
    void
    BidirectionalSerializer::Deserialize(T& value) {
  bool result = mDeserializer->Deserialize(value);

  if (false == result)
    Fail();
}

//////////////////////////////////////////////////
void BidirectionalSerializer::serializeObject(rtti::castable_constptr_t) {
}
void BidirectionalSerializer::deserializeObject(rtti::castable_ptr_t&) {
}

//////////////////////////////////////////////////
void BidirectionalSerializer::serializeObject(rtti::castable_rawconstptr_t) {
}
void BidirectionalSerializer::deserializeObject(rtti::castable_rawptr_t&) {
}

//////////////////////////////////////////////////

template <typename T>
void Serialize(
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

/*template <typename T>
void Serialize(
    std::shared_ptr<const T> const* in, //
    std::shared_ptr<T>* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serialize(*in->get());
  } else {
    bidi.Deserialize(*out->get());
  }
}*/

/*void Serialize(
    rtti::castable_constptr_t const* in, //
    rtti::castable_ptr_t* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serialize((in->get()));
  } else {
    bidi.Deserialize((out->get()));
  }
}*/

/*template <>
void Serialize(
    object_ptr_t const* in, //
    object_ptr_t* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.serializeObject(in->get());
  } else {
    bidi.deserializeObject(*out->get());
  }
}*/

template <>
void Serialize(
    rtti::castable_rawptr_t const* in, //
    rtti::castable_rawptr_t* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    // bidi.serializeObject(*in);
  } else {
    // rtti::ICastable* result = NULL;
    // bidi.deserializeObject(result);
    //*out = result;
  }
}

template <>
void Serialize(
    rtti::castable_rawconstptr_t const* in, //
    rtti::castable_rawconstptr_t* out,
    BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    // bidi.serializeObject(*in);
  } else {
    // rtti::ICastable* result = NULL;
    // bidi.deserializeObject(result);
    //*out = result;
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
