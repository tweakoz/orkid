////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/AccessorObjectArrayPropertyType.h>
#include <ork/reflect/IObjectArrayPropertyType.hpp>

namespace ork { namespace reflect {

template<typename T>
AccessorObjectArrayPropertyType<T>::AccessorObjectArrayPropertyType(
            void (Object::*getter)(T &, size_t index) const,
            void (Object::*setter)(const T &, size_t index), 
            size_t (Object::*counter)() const,
			void (Object::*resizer)(size_t))
    : mGetter(getter)
    , mSetter(setter)
    , mCounter(counter)
	, mResizer(resizer)
{}

template<typename T>
void AccessorObjectArrayPropertyType<T>::Get(T &value, const Object *obj, size_t index) const 
{
    (obj->*mGetter)(value, index);
}

template<typename T>
void AccessorObjectArrayPropertyType<T>::Set(const T &value, Object *obj, size_t index) const 
{
    (obj->*mSetter)(value, index);
}

template<typename T>
size_t AccessorObjectArrayPropertyType<T>::Count(const Object *obj) const
{
    return (obj->*mCounter)();
}

template<typename T>
bool AccessorObjectArrayPropertyType<T>::Resize(Object *obj, size_t size) const
{
	if(mResizer != 0)
	{
		(obj->*mResizer)(size);
		return true;
	}
	else
	{
		return size == Count(obj);
	}
}

} }

