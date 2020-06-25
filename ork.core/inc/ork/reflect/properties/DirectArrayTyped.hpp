////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/DirectArrayTyped.h>
#include <ork/reflect/IObjectArrayPropertyType.hpp>

namespace ork { namespace reflect {

template<typename T>
DirectArrayPropertyType<T>::DirectArrayPropertyType(
        T (Object::*prop)[], size_t size)
    : mProperty(prop)
    , mSize(size)
{}

template<typename T>
void DirectArrayPropertyType<T>::Get(T &value, const Object *obj, size_t index) const
{
    value = (obj->*mProperty)[index];
}

template<typename T>
void DirectArrayPropertyType<T>::Set(const T &value, Object *obj, size_t index) const
{
    (obj->*mProperty)[index] = value;
}

template<typename T>
size_t DirectArrayPropertyType<T>::Count(const Object *) const 
{
    return mSize; 
}

template<typename T>
bool DirectArrayPropertyType<T>::Resize(Object *, size_t size) const
{
	return size == mSize;
}

} }
