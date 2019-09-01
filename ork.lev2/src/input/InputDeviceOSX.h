////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/lev2/input/inputdevice.h>

namespace ork { namespace lev2
{

class InputDeviceOSX : public InputDevice
{
	friend class InputManager;

	public:
    InputDeviceOSX(void);
    ~InputDeviceOSX();

	virtual void Input_Poll(void);
    virtual void Input_Init(void);
	virtual void Input_Configure(void);

	private:
};

} }
