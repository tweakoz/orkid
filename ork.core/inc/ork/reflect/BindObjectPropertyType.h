////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/ITypedProperty.h>
#include <ork/reflect/properties/ITyped.h>
#include <ork/reflect/BindObjectProperty.h>

namespace ork {

class IInputStream;
class IOutputStream;

namespace reflect {

class ISerializer;

template<typename T, typename Interface = ITypedProperty<T>, typename Implementation = ITyped<T> >
class BindObjectPropertyType : public BindObjectProperty<Interface, Implementation>
{
public:
    BindObjectPropertyType(Implementation &prop, Object *obj);

    /*virtual*/ void Get(T &value) const;
    /*virtual*/ void Set(const T &value) const;
};

} }
