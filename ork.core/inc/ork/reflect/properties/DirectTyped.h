////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/properties/ITyped.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template<typename T>
class  DirectPropertyType : public ITyped<T>
{
	DECLARE_TRANSPARENT_TEMPLATE_CASTABLE(DirectPropertyType<T>, ITyped<T>)
    T Object::*mProperty;

public:
    DirectPropertyType(T Object::*);

    /*virtual*/ void Get(T &, const Object *) const;
    /*virtual*/ void Set(const T &, Object *) const;

};

} }
