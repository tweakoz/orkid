////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>

namespace ork { namespace reflect {

template<typename Interface = IProperty, typename Implementation = IObjectProperty>
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

