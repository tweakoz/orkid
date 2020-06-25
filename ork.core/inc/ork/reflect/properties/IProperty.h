////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/rtti/RTTI.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class ISerializer;
class IDeserializer;

class  IProperty : public rtti::ICastable
{
	DECLARE_TRANSPARENT_CASTABLE(IProperty, rtti::ICastable)
public:
    virtual bool Deserialize(IDeserializer &) const = 0;
    virtual bool Serialize(ISerializer &) const = 0;
};

} }

