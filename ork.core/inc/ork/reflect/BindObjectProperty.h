////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/IProperty.h>
#include <ork/reflect/properties/ObjectProperty.h>

namespace ork { namespace reflect {

template<typename Interface = IProperty, typename Implementation = I>
class BindObjectProperty : public Interface
{
public:
    BindObjectProperty(Implementation &prop, Object *obj);

protected:
    Implementation &mObjectProperty;
    Object *mObject;

    /*virtual*/ bool Serialize(ISerializer &serializer) const;
    /*virtual*/ bool Deserialize(IDeserializer &serializer) const;
};

} }

