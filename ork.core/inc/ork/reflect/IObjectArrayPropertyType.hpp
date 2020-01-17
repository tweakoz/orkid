////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectArrayPropertyType.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork { namespace reflect {

template<typename T>
bool IObjectArrayPropertyType<T>::DeserializeItem(IDeserializer &deserializer, Object *obj, size_t index) const
{
	BidirectionalSerializer bidi(deserializer);

	T value;

    bidi | value;

	if(bidi.Succeeded())
       Set(value, obj, index);

    return bidi.Succeeded();
}

template<typename T>
bool IObjectArrayPropertyType<T>::SerializeItem(ISerializer &serializer, const Object *obj, size_t index) const
{
	BidirectionalSerializer bidi(serializer);

    T value;


	Get(value, obj, index);

	bidi | value;
    
	return bidi.Succeeded();
}

} }
