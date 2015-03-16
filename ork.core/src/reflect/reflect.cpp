//////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2010, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IPropertyType.hpp>
#include <ork/reflect/IObjectPropertyType.hpp>

#include <ork/reflect/IObjectArrayProperty.h>
#include <ork/reflect/IObjectMapProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/IObjectArrayPropertyObject.h>

#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/IObjectMapPropertyType.hpp>

#include <ork/reflect/DirectObjectPropertyType.hpp>

#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/AccessorObjectMapPropertyObject.hpp>
#include <ork/reflect/AccessorObjectMapPropertyType.hpp>

#include <ork/reflect/IObjectArrayPropertyType.hpp>
#include <ork/reflect/DirectObjectArrayPropertyType.hpp>
#include <ork/reflect/AccessorObjectArrayPropertyType.hpp>

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

typedef rtti::ICastable *ICastablePointer;

//MACRO(CLASS<long>); \

///////////

#define INSTANTIATE(X) \
namespace ork { namespace reflect {\
	template class X;\
}}

#define BUILD_RTTI_CLASS(X) \
namespace ork { namespace reflect {\
	template<> rtti::Class X::RTTIType::sClass(X::RTTIType::ClassRTTI());\
}}

///////////

//	MACRO(CLASS<CReal>); \

#define FOREACH_PRIMITIVE_TYPE(MACRO,CLASS) \
    MACRO(CLASS<char>); \
    MACRO(CLASS<short>); \
    MACRO(CLASS<int>); \
    MACRO(CLASS<float>); \
    MACRO(CLASS<double>); \
	MACRO(CLASS<PoolString>); \
	MACRO(CLASS<file::Path>); \
	MACRO(CLASS<TransformNode>); \
	MACRO(CLASS<CVector2>); \
	MACRO(CLASS<CVector3>); \
	MACRO(CLASS<CVector4>); \
	MACRO(CLASS<ICastablePointer>);

#define FOREACH_DEPENDENT_TYPE(MACRO,DEPENDENTCLASS,CLASS) \
	MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(char)); \
    MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(short)); \
    MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(int)); \
    MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(long)); \
    MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(float)); \
    MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(double)); \
    MACRO(CONSTRUCT_CLASS_##CLASS##_FOR_##DEPENDENTCLASS(ObjectPointer));

///////////
// Lists //
///////////

//   FOREACH_PRIMITIVE_TYPE(MACRO,IObjectPropertyType);
#define FOREACH_INSTANTIATED_PROPERTY_TYPE(MACRO) \
    FOREACH_PRIMITIVE_TYPE(MACRO,DirectObjectPropertyType); \
    FOREACH_PRIMITIVE_TYPE(MACRO,DirectObjectArrayPropertyType); \
	FOREACH_PRIMITIVE_TYPE(MACRO,AccessorObjectPropertyType); \
	FOREACH_PRIMITIVE_TYPE(MACRO,AccessorObjectArrayPropertyType);

#define FOREACH_RTTI_PROPERTY_TYPE(MACRO) \
	MACRO(IProperty); \
	MACRO(IObjectProperty); \
	MACRO(IObjectPropertyObject); \
	MACRO(IObjectArrayProperty); \
	MACRO(IObjectArrayPropertyObject); \
	MACRO(IObjectMapProperty); \
	MACRO(IObjectMapPropertyObject);

//	MACRO(IObjectArrayPropertyObject); \

////////////////////
// Instantiations //
////////////////////

namespace ork { 
ISerializer::~ISerializer() {}
IDeserializer::~IDeserializer() {}

template class orklut<ConstString, IObjectProperty *>;
//template class orklut<ork::PoolString,ork::rtti::Class*>;
template class orklut<ConstString, IObjectFunctor *>;
template class orklut<ConstString, object::Signal Object:: *>;
template class orklut<ConstString, object::AutoSlot Object:: *>;
}

//"ork::reflect::AccessorObjectPropertyType<int>::AccessorObjectPropertyType(void (ork::Object::*)(int&) const, void (ork::Object::*)(int const&))", referenced from:
//ork::reflect::AccessorObjectPropertyType<int>& ork::reflect::RegisterProperty<TestObject, int>(char const*, void (TestObject::*)(int&) const, void (TestObject::*)(int const&), ork::reflect::Description&)in main.o
//"ork::reflect::DirectObjectPropertyType<int>::DirectObjectPropertyType(int ork::Object::*)", referenced from:
//ork::reflect::DirectObjectPropertyType<int>& ork::reflect::RegisterProperty<TestSubObject, int>(char const*, int TestSubObject::*, ork::reflect::Description&)in main.o

//template class AccessorObjectPropertyType<int>;
//template class DirectObjectPropertyType<int>;

FOREACH_INSTANTIATED_PROPERTY_TYPE(INSTANTIATE);
FOREACH_RTTI_PROPERTY_TYPE(INSTANTIATE_TRANSPARENT_CASTABLE);

namespace ork { namespace reflect {
template class IObjectPropertyType<int>;
template class IObjectPropertyType<float>;
template class IObjectPropertyType<CVector2>;
template class IObjectPropertyType<CVector3>;
template class IObjectPropertyType<CVector4>;
template class IObjectPropertyType<CMatrix4>;
template class IObjectPropertyType<ork::rtti::ICastable*>;
template class IObjectPropertyType<ork::Object*>;
template class IObjectPropertyType<ork::file::Path>;
template class IObjectPropertyType<PoolString>;
template class IObjectPropertyType<bool>;
template class DirectObjectPropertyType<bool>;
template class DirectObjectPropertyType<ork::Object*>;
template class IObjectPropertyType<Char4>;
template class DirectObjectPropertyType<Char4>;
template class IObjectPropertyType<Char8>;
template class DirectObjectPropertyType<Char8>;
template class DirectObjectMapPropertyType<orkmap<int,int> >;
template class DirectObjectMapPropertyType<orklut<int,int> >;
template class DirectObjectMapPropertyType<orkmap<PoolString,PoolString> >;
template class DirectObjectMapPropertyType<orklut<PoolString,PoolString> >;
template class AccessorObjectMapPropertyObject<int>;
template class AccessorObjectMapPropertyType<int, char>;
template class AccessorObjectMapPropertyType<int, ICastablePointer>;
template class DirectObjectPropertyType<CMatrix4>;
template class DirectObjectPropertyType< TQuaternion<float> >;

template class DirectObjectMapPropertyType<orkmap< float, CVector4> >;
//template class DirectObjectPropertyType<orkvector<CVector2> >;

bool SetInvokationParameter(IInvokation *invokation, int param, const char *paramdata)
{
	if(param >= invokation->GetNumParameters())
		return false;

	ork::stream::StringInputStream stream(paramdata);
	ork::reflect::serialize::TextDeserializer deser(stream);
	BidirectionalSerializer bidi(deser);
	invokation->ApplyParam(bidi, param);
	return bidi.Succeeded();
}
}}
