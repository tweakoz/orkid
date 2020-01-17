////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectPropertyType.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>

namespace ork { namespace reflect {

template<typename T>
bool IObjectPropertyType<T>::Deserialize(IDeserializer &deserializer, Object *obj) const
{
    T value;

	BidirectionalSerializer bidi(deserializer);

	bidi | value;

	bool result = bidi.Succeeded();

    if(result)
    {
        Set(value, obj);
    }

    return result;
}

template<typename T>
bool IObjectPropertyType<T>::Serialize(ISerializer &serializer, const Object *obj) const
{
    T value;
    Get(value, obj);

	BidirectionalSerializer bidi(serializer);

	bidi | value;

    return bidi.Succeeded();
}

template<typename T>
typename IObjectPropertyType<T>::RTTIType::RTTICategory IObjectPropertyType<T>::sClass( IObjectPropertyType<T>::RTTIType::ClassRTTI() );

template<typename T>
typename IObjectPropertyType<T>::RTTIType::RTTICategory* IObjectPropertyType<T>::GetClassStatic()
{
	return &sClass;
}

template<typename T>
typename IObjectPropertyType<T>::RTTIType::RTTICategory* IObjectPropertyType<T>::GetClass() const
{
	return GetClassStatic();
}

} }

