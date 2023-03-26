////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
