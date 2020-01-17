////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/DirectObjectVectorPropertyType.h>
#include <ork/reflect/IObjectArrayPropertyType.hpp>

namespace ork { namespace reflect {

template<typename VectorType>
DirectObjectVectorPropertyType<VectorType>::DirectObjectVectorPropertyType(
	VectorType Object::*prop)
	: mProperty(prop)
{}

template<typename VectorType>
void DirectObjectVectorPropertyType<VectorType>::Get(typename VectorType::value_type &value, const Object *object, size_t index) const
{
	value = (object->*mProperty)[index];
}

template<typename VectorType>
void DirectObjectVectorPropertyType<VectorType>::Set(const typename VectorType::value_type &value, Object *object, size_t index) const
{
	(object->*mProperty)[index] = value;
}

template<typename VectorType>
size_t DirectObjectVectorPropertyType<VectorType>::Count(const Object *object) const
{
	return size_t((object->*mProperty).size());
}

template<typename VectorType>
bool DirectObjectVectorPropertyType<VectorType>::Resize(Object *object, size_t size) const
{
	(object->*mProperty).resize(size);
	return Count(object) == size;
}

} }

