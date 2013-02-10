////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/input/CInputDeviceDummy.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

CInputDeviceDummy::CInputDeviceDummy() : CInputDevice()
{
	orkprintf("created Dummy Input Device\n");
}

CInputDeviceDummy::~CInputDeviceDummy()
{
}

} }
