////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
