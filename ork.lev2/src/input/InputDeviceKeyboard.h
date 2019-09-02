////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/input/inputdevice.h>

namespace ork { namespace lev2 {

class InputDeviceKeyboard : public ork::lev2::InputDevice
{
public:
    InputDeviceKeyboard();

	virtual void Input_poll();

	 orkmultimap<int, int> mKeyboardInputMap;
};

} }
