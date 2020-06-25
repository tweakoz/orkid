////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/ITyped.hpp>

namespace ork { namespace reflect {

class ISerializer;

template<typename T>
DirectPropertyType<T>::DirectPropertyType(T Object::*property)
	: mProperty(property)
{}

template<typename T>
void DirectPropertyType<T>::Get(T &value, const Object *obj) const 
{
    value = obj->*mProperty;
}

template<typename T>
void DirectPropertyType<T>::Set(const T &value, Object *obj) const 
{
    obj->*mProperty = value;
}

template<typename T>
typename DirectPropertyType<T>::RTTITyped::RTTICategory DirectPropertyType<T>::sClass( DirectPropertyType<T>::RTTITyped::ClassRTTI() );

template<typename T>
typename DirectPropertyType<T>::RTTITyped::RTTICategory* DirectPropertyType<T>::GetClassStatic()
{
	return &sClass;
}

template<typename T>
typename DirectPropertyType<T>::RTTITyped::RTTICategory* DirectPropertyType<T>::GetClass() const
{
	return GetClassStatic();
}

//INSTANTIATE_TRANSPARENT_TEMPLATE_CASTABLE( ClassName )
//	ClassName::RTTITyped::RTTIClassClass ClassName::sClass(ClassName::RTTITyped::ClassRTTI()); \
//	ClassName::RTTITyped::RTTIClassClass *ClassName::GetClassStatic() { return &sClass; } \
//	rtti::Class *ClassName::GetClass() const { return GetClassStatic(); }

} }
