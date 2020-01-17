////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IPropertyType.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork { namespace reflect {

template<typename T>
/*virtual*/ bool IPropertyType<T>::Deserialize(IDeserializer &serializer) const
{
    T value;
    bool result = serializer.Deserialize(value);

    if(result)
    {
        Set(value);
    }

    return result;
}

template<typename T>
/*virtual*/ bool IPropertyType<T>::Serialize(ISerializer &serializer) const
{
    T value;
    Get(value);

	BidirectionalSerializer bidi(serializer);

	bidi | value;

//    bool result = serializer.Serialize(value);

    return bidi.Succeeded();
}

} }

