////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <cstddef>

#include <ork/reflect/IObjectMapProperty.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class  IObjectMapPropertyObject : public IObjectMapProperty
{
	DECLARE_TRANSPARENT_CASTABLE(IObjectMapPropertyObject, IObjectMapProperty)
public:
	virtual Object *AccessItem(IDeserializer &key, int, Object *) const = 0;
    virtual const Object *AccessItem(IDeserializer &key, int, const Object *) const = 0;
};

} }

