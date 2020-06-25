////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectProperty.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class  AccessorVariant : public IObjectProperty
{
	static void GetClassStatic(); // Kill inherited GetClassStatic()
public:
    AccessorVariant(
		bool (Object::*getter)(ISerializer &) const,
		bool (Object::*setter)(IDeserializer &));
private:
    /*virtual*/ bool Deserialize(IDeserializer &, Object *) const;
    /*virtual*/ bool Serialize(ISerializer &, const Object *) const;
    bool (Object::*mSerialize)(ISerializer &) const;
    bool (Object::*mDeserialize)(IDeserializer &);
};

} }

