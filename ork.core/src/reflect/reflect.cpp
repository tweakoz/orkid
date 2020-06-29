//////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2020, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/ITyped.hpp>

#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/properties/IMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/IObjectArray.h>

#include <ork/reflect/properties/ITyped.hpp>
#include <ork/reflect/properties/ITypedMap.hpp>

#include <ork/reflect/properties/DirectTyped.hpp>

#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/AccessorObjectMap.hpp>
#include <ork/reflect/properties/AccessorTypedMap.hpp>

#include <ork/reflect/properties/ITypedArray.hpp>
#include <ork/reflect/properties/DirectTypedArray.hpp>
#include <ork/reflect/properties/AccessorTypedArray.hpp>

#include <ork/kernel/string/ConstString.h>

#include <ork/reflect/Serialize.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/rtti/downcast.h>

#include <ork/reflect/Functor.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/TransformNode.h>

#include <ork/stream/StringInputStream.h>

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

//MACRO(CLASS<long>); \

///////////

#define INSTANTIATE(X)                                                                                                             \
  namespace ork { namespace reflect {                                                                                              \
  template class X;                                                                                                                \
  }                                                                                                                                \
  }

#define BUILD_RTTI_CLASS(X)                                                                                                        \
  namespace ork { namespace reflect {                                                                                              \
  template <> rtti::Class X::RTTITyped::sClass(X::RTTITyped::ClassRTTI());                                                         \
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
  MACRO(CLASS<rtti::castable_rawptr_t>);

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
  FOREACH_PRIMITIVE_TYPE(MACRO, DirectTyped);                                                                                      \
  FOREACH_PRIMITIVE_TYPE(MACRO, DirectTypedArray);                                                                                 \
  FOREACH_PRIMITIVE_TYPE(MACRO, AccessorTyped);                                                                                    \
  FOREACH_PRIMITIVE_TYPE(MACRO, AccessorTypedArray);

//	MACRO(IObjectArray); \

////////////////////
// Instantiations //
////////////////////

namespace ork {
ISerializer::~ISerializer() {
}
IDeserializer::~IDeserializer() {
}

template class orklut<ConstString, ObjectProperty*>;
template class orklut<ConstString, IObjectFunctor*>;
template class orklut<ConstString, object::Signal Object::*>;
template class orklut<ConstString, object::AutoSlot Object::*>;
} // namespace ork

FOREACH_INSTANTIATED_PROPERTY_TYPE(INSTANTIATE);

namespace ork { namespace reflect {
template class ITyped<int>;
template class ITyped<float>;
template class ITyped<fvec2>;
template class ITyped<fvec3>;
template class ITyped<fvec4>;
template class ITyped<fmtx4>;
template class ITyped<rtti::castable_rawptr_t>;
template class ITyped<ork::file::Path>;
template class ITyped<PoolString>;
template class ITyped<std::string>;
template class ITyped<bool>;
template class DirectTyped<bool>;
template class ITyped<Char4>;
template class DirectTyped<Char4>;
template class ITyped<Char8>;
template class DirectTyped<Char8>;
template class DirectTypedMap<orkmap<int, int>>;
template class DirectTypedMap<orklut<int, int>>;
template class DirectTypedMap<orkmap<PoolString, PoolString>>;
template class DirectTypedMap<orklut<PoolString, PoolString>>;
template class DirectTypedMap<orkmap<std::string, std::string>>;
template class DirectTypedMap<orklut<std::string, std::string>>;
template class AccessorObjectMap<int>;
template class AccessorTypedMap<int, char>;
template class AccessorTypedMap<int, rtti::castable_rawptr_t>;
template class DirectTyped<fmtx4>;
template class DirectTyped<Quaternion<float>>;
template class ITyped<TransformNode>;
template class DirectTypedMap<orkmap<float, fvec4>>;
// template class DirectTyped<orkvector<fvec2> >;
// template class ITyped<ork::Object*>;
// template class DirectTyped<ork::Object*>;

bool SetInvokationParameter(IInvokation* invokation, int param, const char* paramdata) {
  if (param >= invokation->GetNumParameters())
    return false;
  /*
ork::stream::StringInputStream stream(paramdata);
ork::reflect::serdes::TextDeserializer deser(stream);
BidirectionalSerializer bidi(deser);
invokation->ApplyParam(bidi, param);
return bidi.Succeeded();*/
  return false;
}

}} // namespace ork::reflect
