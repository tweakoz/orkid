////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/input/inputdevice.h>

namespace ork { namespace lev2
{

class InputDeviceDummy : public InputDevice
{
	friend struct InputManager;

	public:
    InputDeviceDummy(void);
	static void ClassInit( CClass *pClass );
	static std::string GetClassName( void ) { return std::string("InputDeviceDummy"); }
    ~InputDeviceDummy();

	private:
};

} }

///////////////////////////////////////////////////////////////////////////////
