////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectArrayPropertyType.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template<typename T>
class  DirectObjectArrayPropertyType : public IObjectArrayPropertyType<T>
{
public:
    DirectObjectArrayPropertyType(T (Object::*)[], size_t);
    /*virtual*/ void Get(T &, const Object *, size_t) const;
    /*virtual*/ void Set(const T &, Object *, size_t) const;
    /*virtual*/ size_t Count(const Object *) const;
	/*virtual*/ bool Resize(Object *obj, size_t size) const;
private:
    T (Object::*mProperty)[];
    size_t mSize;
};

} }
