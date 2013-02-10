////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_LEV2_INPUTDEVICEXINPUT_H
#define ORK_LEV2_INPUTDEVICEXINPUT_H

#if defined(_WIN32) || defined(_XBOX)

#include <ork/lev2/input/input.h>

namespace ork { namespace lev2 {

class CInputDeviceXInput : public ork::lev2::CInputDevice
{
public:
	CInputDeviceXInput() : mdwUserIndex(0xFFFFFFFF) {}

    virtual void Input_Poll();

	/*virtual*/ void RumbleClear() ;
	/*virtual*/ void RumbleTrigger(int amount) ; //e

	


	void SetUserIndex(DWORD dwUserIndex) { mdwUserIndex = dwUserIndex; }
	DWORD GetUserIndex() const { return mdwUserIndex; }


private:
	DWORD mdwUserIndex;

	float m_fLeftMotorSpeed;
	float m_fRightMotorSpeed;

	int m_Rumbletimer;
#if defined(_XBOX)
	XINPUT_VIBRATION mVibration;
#endif

};

} }

#endif // _WIN32 || _XBOX

#endif // ORK_LEV2_INPUTDEVICEXINPUT_H
