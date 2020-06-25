////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectPropertyObject.h>

#include <ork/config/config.h>

namespace ork { 
    
class Object;

namespace reflect {

class  AccessorPropertyObject : public IObjectPropertyObject
{
public:
    AccessorPropertyObject(Object *(Object::*)());

    /*virtual*/ bool Serialize(ISerializer &, const Object *) const;
    /*virtual*/ bool Deserialize(IDeserializer &, Object *) const;
	/*virtual*/ Object *Access(Object *) const;
	/*virtual*/ const Object *Access(const Object *) const;
private:
    Object *(Object::*mObjectAccessor)();
};

} }
