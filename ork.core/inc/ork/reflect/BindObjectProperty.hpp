////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/BindObjectProperty.h>

namespace ork { namespace reflect {

template<typename Interface, typename Implementation>
BindObjectProperty<Interface,Implementation>::BindObjectProperty(Implementation &prop, Object *obj)
    : mObjectProperty(prop)
	, mObject(obj)
{}

template<typename Interface, typename Implementation>
bool BindObjectProperty<Interface,Implementation>::Serialize(ISerializer &serializer) const
{
    return static_cast<IObjectProperty&>(mObjectProperty).Serialize(serializer, mObject);
}

template<typename Interface, typename Implementation>
bool BindObjectProperty<Interface,Implementation>::Deserialize(IDeserializer &serializer) const
{
    return static_cast<IObjectProperty&>(mObjectProperty).Deserialize(serializer, mObject);
}

} }

