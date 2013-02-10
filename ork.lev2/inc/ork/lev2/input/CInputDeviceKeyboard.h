////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_LEV2_INPUTDEVICEKEYBOARD_H
#define ORK_LEV2_INPUTDEVICEKEYBOARD_H

#include <ork/lev2/input/input.h>

namespace ork { namespace lev2 {

class CInputDeviceKeyboard : public ork::lev2::CInputDevice
{
public:
    CInputDeviceKeyboard();

	virtual void Input_Poll();

	 orkmultimap<int, int> mKeyboardInputMap;
};

} }

#endif // ORK_LEV2_INPUTDEVICEKEYBOARD_H
