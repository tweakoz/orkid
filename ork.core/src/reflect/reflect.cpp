//////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2020, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IObjectPropertySharedObject.h>
#include <ork/reflect/IPropertyType.hpp>
#include <ork/reflect/IObjectPropertyType.hpp>

#include <ork/reflect/IObjectArrayProperty.h>
#include <ork/reflect/IObjectMapProperty.h>
#include <ork/reflect/properties/DirectMapTyped.hpp>
#include <ork/reflect/IObjectArrayPropertyObject.h>

#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/IObjectMapPropertyType.hpp>

#include <ork/reflect/properties/DirectTyped.hpp>

#include <ork/reflect/properties/AccessorPropertyType.hpp>
#include <ork/reflect/properties/AccessorMapPropertyObject.hpp>
#include <ork/reflect/properties/AccessorMapPropertyType.hpp>

#include <ork/reflect/IObjectArrayPropertyType.hpp>
#include <ork/reflect/properties/DirectArrayTyped.hpp>
#include <ork/reflect/properties/AccessorArrayPropertyType.hpp>

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
  template <> rtti::Class X::RTTIType::sClass(X::RTTIType::ClassRTTI());                                                           \
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

//   FOREACH_PRIMITIVE_TYPE(MACRO,IObjectPropertyType);
#define FOREACH_INSTANTIATED_PROPERTY_TYPE(MACRO)                                                                                  \
  FOREACH_PRIMITIVE_TYPE(MACRO, DirectPropertyType);                                                                         \
  FOREACH_PRIMITIVE_TYPE(MACRO, DirectArrayPropertyType);                                                                    \
  FOREACH_PRIMITIVE_TYPE(MACRO, AccessorPropertyType);                                                                       \
  FOREACH_PRIMITIVE_TYPE(MACRO, AccessorArrayPropertyType);

#define FOREACH_RTTI_PROPERTY_TYPE(MACRO)                                                                                          \
  MACRO(IProperty);                                                                                                                \
  MACRO(IObjectProperty);                                                                                                          \
  MACRO(IObjectPropertyObject);                                                                                                    \
  MACRO(IObjectPropertySharedObject);                                                                                              \
  MACRO(IObjectArrayProperty);                                                                                                     \
  MACRO(IObjectArrayPropertyObject);                                                                                               \
  MACRO(IObjectMapProperty);                                                                                                       \
  MACRO(IObjectMapPropertyObject);

//	MACRO(IObjectArrayPropertyObject); \

////////////////////
// Instantiations //
////////////////////

namespace ork {
ISerializer::~ISerializer() {
}
IDeserializer::~IDeserializer() {
}

template class orklut<ConstString, IObjectProperty*>;
// template class orklut<ork::PoolString,ork::rtti::Class*>;
template class orklut<ConstString, IObjectFunctor*>;
template class orklut<ConstString, object::Signal Object::*>;
template class orklut<ConstString, object::AutoSlot Object::*>;
} // namespace ork

//"ork::reflect::AccessorPropertyType<int>::AccessorPropertyType(void (ork::Object::*)(int&) const, void
//(ork::Object::*)(int const&))", referenced from: ork::reflect::AccessorPropertyType<int>&
// ork::reflect::RegisterProperty<TestObject, int>(char const*, void (TestObject::*)(int&) const, void (TestObject::*)(int const&),
// ork::reflect::Description&)in main.o "ork::reflect::DirectPropertyType<int>::DirectPropertyType(int ork::Object::*)",
// referenced from: ork::reflect::DirectPropertyType<int>& ork::reflect::RegisterProperty<TestSubObject, int>(char const*, int
// TestSubObject::*, ork::reflect::Description&)in main.o

// template class AccessorPropertyType<int>;
// template class DirectPropertyType<int>;

FOREACH_INSTANTIATED_PROPERTY_TYPE(INSTANTIATE);
FOREACH_RTTI_PROPERTY_TYPE(INSTANTIATE_TRANSPARENT_CASTABLE);

namespace ork { namespace reflect {
template class IObjectPropertyType<int>;
template class IObjectPropertyType<float>;
template class IObjectPropertyType<fvec2>;
template class IObjectPropertyType<fvec3>;
template class IObjectPropertyType<fvec4>;
template class IObjectPropertyType<fmtx4>;
template class IObjectPropertyType<ork::rtti::ICastable*>;
template class IObjectPropertyType<ork::Object*>;
template class IObjectPropertyType<ork::file::Path>;
template class IObjectPropertyType<PoolString>;
template class IObjectPropertyType<std::string>;
template class IObjectPropertyType<bool>;
template class DirectPropertyType<bool>;
template class DirectPropertyType<ork::Object*>;
template class IObjectPropertyType<Char4>;
template class DirectPropertyType<Char4>;
template class IObjectPropertyType<Char8>;
template class DirectPropertyType<Char8>;
template class DirectMapPropertyType<orkmap<int, int>>;
template class DirectMapPropertyType<orklut<int, int>>;
template class DirectMapPropertyType<orkmap<PoolString, PoolString>>;
template class DirectMapPropertyType<orklut<PoolString, PoolString>>;
template class DirectMapPropertyType<orkmap<std::string, std::string>>;
template class DirectMapPropertyType<orklut<std::string, std::string>>;
template class AccessorMapPropertyObject<int>;
template class AccessorMapPropertyType<int, char>;
template class AccessorMapPropertyType<int, ICastablePointer>;
template class DirectPropertyType<fmtx4>;
template class DirectPropertyType<Quaternion<float>>;
template class IObjectPropertyType<TransformNode>;
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
