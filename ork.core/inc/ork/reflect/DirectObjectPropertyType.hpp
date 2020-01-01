////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/DirectObjectPropertyType.h>
#include <ork/reflect/IObjectPropertyType.hpp>

namespace ork { namespace reflect {

class ISerializer;

template<typename T>
DirectObjectPropertyType<T>::DirectObjectPropertyType(T Object::*property)
	: mProperty(property)
{}

template<typename T>
void DirectObjectPropertyType<T>::Get(T &value, const Object *obj) const 
{
    value = obj->*mProperty;
}

template<typename T>
void DirectObjectPropertyType<T>::Set(const T &value, Object *obj) const 
{
    obj->*mProperty = value;
}

template<typename T>
typename DirectObjectPropertyType<T>::RTTIType::RTTICategory DirectObjectPropertyType<T>::sClass( DirectObjectPropertyType<T>::RTTIType::ClassRTTI() );

template<typename T>
typename DirectObjectPropertyType<T>::RTTIType::RTTICategory* DirectObjectPropertyType<T>::GetClassStatic()
{
	return &sClass;
}

template<typename T>
typename DirectObjectPropertyType<T>::RTTIType::RTTICategory* DirectObjectPropertyType<T>::GetClass() const
{
	return GetClassStatic();
}

//INSTANTIATE_TRANSPARENT_TEMPLATE_CASTABLE( ClassName )
//	ClassName::RTTIType::RTTIClassClass ClassName::sClass(ClassName::RTTIType::ClassRTTI()); \
//	ClassName::RTTIType::RTTIClassClass *ClassName::GetClassStatic() { return &sClass; } \
//	rtti::Class *ClassName::GetClass() const { return GetClassStatic(); }

} }
