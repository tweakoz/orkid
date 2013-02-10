////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/BindObjectPropertyType.h>
#include <ork/reflect/BindObjectProperty.hpp>

namespace ork { namespace reflect {

template<typename T, typename Interface, typename Implementation>
BindObjectPropertyType<T, Interface, Implementation>::BindObjectPropertyType(Implementation &prop, Object *obj)
    : BindObjectProperty<Interface, Implementation>(prop, obj)
{}

template<typename T, typename Interface, typename Implementation>
void BindObjectPropertyType<T,Interface,Implementation>::Get(T &value) const 
{
    typedef BindObjectProperty<Interface, Implementation> BaseType;
    BaseType::mObjectProperty.Get(value, BaseType::mObject);
}

template<typename T, typename Interface, typename Implementation>
void BindObjectPropertyType<T,Interface,Implementation>::Set(const T &value) const
{
    typedef BindObjectProperty<Interface, Implementation> BaseType;
    BaseType::mObjectProperty.Set(value, BaseType::mObject);
}

} }

