//////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2020, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/ISharedObject.h>
#include <ork/reflect/IPropertyType.hpp>
#include <ork/reflect/properties/ITyped.hpp>

#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/properties/IMap.h>
#include <ork/reflect/properties/DirectMapTyped.hpp>
#include <ork/reflect/properties/IObjectArray.h>

#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/ITypedMap.hpp>

#include <ork/reflect/properties/DirectTyped.hpp>

#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/AccessorMapObject.hpp>
#include <ork/reflect/properties/AccessorMapType.hpp>

#include <ork/reflect/properties/ITypedArray.hpp>
#include <ork/reflect/properties/DirectArrayTyped.hpp>
#include <ork/reflect/properties/AccessorArrayType.hpp>

#include <ork/kernel/string/ConstString.h>

#include <ork/reflect/Serialize.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/rtti/downcast.h>

#include <ork/reflect/Functor.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/TransformNode.h>

#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/TextDeserializer.h>

#include <map>

#include <ork/kernel/orklut.hpp>
#include <ork/object/Object.h>

#include <ork/math/cvector4.h>

#include <ork/file/file.h>

using namespace ork;

using namespace reflect;

///////////////////
// Configuration //
///////////////////

typedef rtti::ICastable* ICastablePointer;

//MACRO(CLASS<long>); \

///////////

#define INSTANTIATE(X)                                                                                                             \
  namespace ork { namespace reflect {                                                                                              \
  template class X;                                                                                                                \
  }                                                                                                                                \
  }

#define BUILD_RTTI_CLASS(X)                                                                                                        \
  namespace ork { namespace reflect {                                                                                              \
  template <> rtti::Class X::RTTITyped::sClass(X::RTTITyped::ClassRTTI());                                                           \
  }                                                                                                                                \
  }

///////////

//	MACRO(CLASS<float>); \

#define FOREACH_PRIMITIVE_TYPE(MACRO, CLASS)                                                                                       \
  MACRO(CLASS<char>);                                                                                                              \
  MACRO(CLASS<short>);                                                                                                             \
  MACRO(CLASS<int>);                                                                                                               \
  MACRO(CLASS<float>);                                                                                                             \
  MACRO(CLASS<double>);                                                                                                            \
  MACRO(CLASS<std::string>);                                                                                                       \
  MACRO(CLASS<PoolString>);                                                                                                        \
  MACRO(CLASS<file::Path>);                                                                                                        \
  MACRO(CLASS<TransformNode>);                                                                                                     \
  MACRO(CLASS<fvec2>);                                                                                                             \
  MACRO(CLASS<fvec3>);                                                                                                             \
  MACRO(CLASS<fvec4>);                                                                                                             \
  MACRO(CLASS<ICastablePointer>);

#define FOREACH_DEPENDENT_TYPE(MACRO, DEPENDENTCLASS, CLASS)                                                                       \
  MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(char));                                                                     \
  MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(short));                                                                    \
  MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(int));                                                                      \
  MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(long));                                                                     \
  MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(float));                                                                    \
  MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(double));                                                                   \
  MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(ObjectPointer));

///////////
// Lists //
///////////

//   FOREACH_PRIMITIVE_TYPE(MACRO,ITyped);
#define FOREACH_INSTANTIATED_PROPERTY_TYPE(MACRO)                                                                                  \
  FOREACH_PRIMITIVE_TYPE(MACRO, DirectPropertyType);                                                                         \
  FOREACH_PRIMITIVE_TYPE(MACRO, DirectArrayPropertyType);                                                                    \
  FOREACH_PRIMITIVE_TYPE(MACRO, AccessorTyped);                                                                       \
  FOREACH_PRIMITIVE_TYPE(MACRO, AccessorArrayType);

#define FOREACH_RTTI_PROPERTY_TYPE(MACRO)                                                                                          \
  MACRO(IProperty);                                                                                                                \
  MACRO(I);                                                                                                          \
  MACRO(IObject);                                                                                                    \
  MACRO(ISharedObject);                                                                                              \
  MACRO(IArray);                                                                                                     \
  MACRO(IObjectArray);                                                                                               \
  MACRO(IMap);                                                                                                       \
  MACRO(IObjectMap);

//	MACRO(IObjectArray); \

////////////////////
// Instantiations //
////////////////////

namespace ork {
ISerializer::~ISerializer() {
}
IDeserializer::~IDeserializer() {
}

template class orklut<ConstString, I*>;
// template class orklut<ork::PoolString,ork::rtti::Class*>;
template class orklut<ConstString, IObjectFunctor*>;
template class orklut<ConstString, object::Signal Object::*>;
template class orklut<ConstString, object::AutoSlot Object::*>;
} // namespace ork

//"ork::reflect::AccessorTyped<int>::AccessorTyped(void (ork::Object::*)(int&) const, void
//(ork::Object::*)(int const&))", referenced from: ork::reflect::AccessorTyped<int>&
// ork::reflect::RegisterProperty<TestObject, int>(char const*, void (TestObject::*)(int&) const, void (TestObject::*)(int const&),
// ork::reflect::Description&)in main.o "ork::reflect::DirectPropertyType<int>::DirectPropertyType(int ork::Object::*)",
// referenced from: ork::reflect::DirectPropertyType<int>& ork::reflect::RegisterProperty<TestSubObject, int>(char const*, int
// TestSubObject::*, ork::reflect::Description&)in main.o

// template class AccessorTyped<int>;
// template class DirectPropertyType<int>;

FOREACH_INSTANTIATED_PROPERTY_TYPE(INSTANTIATE);
FOREACH_RTTI_PROPERTY_TYPE(INSTANTIATE_TRANSPARENT_CASTABLE);

namespace ork { namespace reflect {
template class ITyped<int>;
template class ITyped<float>;
template class ITyped<fvec2>;
template class ITyped<fvec3>;
template class ITyped<fvec4>;
template class ITyped<fmtx4>;
template class ITyped<ork::rtti::ICastable*>;
template class ITyped<ork::Object*>;
template class ITyped<ork::file::Path>;
template class ITyped<PoolString>;
template class ITyped<std::string>;
template class ITyped<bool>;
template class DirectPropertyType<bool>;
template class DirectPropertyType<ork::Object*>;
template class ITyped<Char4>;
template class DirectPropertyType<Char4>;
template class ITyped<Char8>;
template class DirectPropertyType<Char8>;
template class DirectMapPropertyType<orkmap<int, int>>;
template class DirectMapPropertyType<orklut<int, int>>;
template class DirectMapPropertyType<orkmap<PoolString, PoolString>>;
template class DirectMapPropertyType<orklut<PoolString, PoolString>>;
template class DirectMapPropertyType<orkmap<std::string, std::string>>;
template class DirectMapPropertyType<orklut<std::string, std::string>>;
template class AccessorMapObject<int>;
template class AccessorMapType<int, char>;
template class AccessorMapType<int, ICastablePointer>;
template class DirectPropertyType<fmtx4>;
template class DirectPropertyType<Quaternion<float>>;
template class ITyped<TransformNode>;
template class DirectMapPropertyType<orkmap<float, fvec4>>;
// template class DirectPropertyType<orkvector<fvec2> >;

bool SetInvokationParameter(IInvokation* invokation, int param, const char* paramdata) {
  if (param >= invokation->GetNumParameters())
    return false;

  ork::stream::StringInputStream stream(paramdata);
  ork::reflect::serialize::TextDeserializer deser(stream);
  BidirectionalSerializer bidi(deser);
  invokation->ApplyParam(bidi, param);
  return bidi.Succeeded();
}
}} // namespace ork::reflect
