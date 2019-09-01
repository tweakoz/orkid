////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include "InputDeviceDummy.h"

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

InputDeviceDummy::InputDeviceDummy() : InputDevice()
{
	orkprintf("created Dummy Input Device\n");
}

InputDeviceDummy::~InputDeviceDummy()
{
}

} }
