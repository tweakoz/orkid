////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/lev2/input/inputdevice.h>

namespace ork { namespace lev2
{

class InputDeviceOSX : public InputDevice
{
	friend struct InputManager;

	public:
    InputDeviceOSX(void);
    ~InputDeviceOSX();

	virtual void Input_Poll(void);
    virtual void Input_Init(void);
	virtual void Input_Configure(void);

	private:
};

} }
