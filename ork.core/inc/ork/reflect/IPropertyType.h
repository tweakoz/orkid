////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IProperty.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template<typename T>
class  IPropertyType : public IProperty
{
	static void GetClassStatic(); // Kill inherited GetClassStatic()
public:
    virtual void Get(T &) const = 0;
    virtual void Set(const T &) const = 0;
private:
    /*virtual*/ bool Deserialize(IDeserializer &) const;
    /*virtual*/ bool Serialize(ISerializer &) const;
};

} }

