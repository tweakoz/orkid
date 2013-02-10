////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/config/config.h>

namespace ork { namespace rtti {

class Class;

class  ICastable
{
public:

	virtual ~ICastable() {}
	virtual Class *GetClass() const = 0;
	static Class *GetClassStatic();

	struct RTTIType { typedef Class RTTICategory; };
};

} }
