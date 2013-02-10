////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined(_WIN32) || defined(_XBOX)

#include <ork/lev2/input/CInputDeviceXInput.h>

#if ! defined(_XBOX)
#include <XInput.h>
#pragma comment(lib, "xinput.lib")
#endif

namespace ork { namespace lev2 {

void CInputDeviceXInput::RumbleClear()
{
	 m_fLeftMotorSpeed = 0.0f;
	 m_fRightMotorSpeed = 0.0f;

	 m_Rumbletimer = 0;

#if defined(_XBOX)
	mVibration.wLeftMotorSpeed = 0;
	mVibration.wRightMotorSpeed = 0;
    XInputSetState( mdwUserIndex, &mVibration );
#endif
}

void CInputDeviceXInput::RumbleTrigger(int amount)
{

	switch ( amount)
	{
		case 0 : //e boost
	 		m_fLeftMotorSpeed = 0.0f;
	 		m_fRightMotorSpeed = 0.6f;
	 		m_Rumbletimer = 15;
	   	break;
		case 1 : //Hot by enemy
	 		m_fLeftMotorSpeed = 1.0f;
	 		m_fRightMotorSpeed = 0.0f;
	 		m_Rumbletimer = 18;
	   	break;
		case 2 :
	 		m_fLeftMotorSpeed = 0.6f;
	 		m_fRightMotorSpeed = 0.6f;
	 		m_Rumbletimer = 15;
	   	break;

	}

}


void CInputDeviceXInput::Input_Poll()
{
	if(mdwUserIndex < 0 || mdwUserIndex > 3)
	{
		mConnectionStatus = CONN_STATUS_UNKNOWN;
		return;
	}

	// TODO: On Windows, move this to app/window ACTIVATION/DEACTIVATION
	// TODO: On XBOX, where does this go?
#if ! defined(_XBOX)
	static bool binit = true;
	if( binit )
	{
	//et 	XInputEnable(TRUE);
		binit = false;
	}
#endif

	XINPUT_STATE XInputState;
	DWORD dwResult = XInputGetState(mdwUserIndex, &XInputState);
	XINPUT_GAMEPAD &gamepad = XInputState.Gamepad;

	// Update Connection Status
	if(dwResult == ERROR_SUCCESS)
		Connect();
	else
		Disconnect();

	for(int i = ETRIG_RAW_BEGIN; i <= ETRIG_RAW_END; ++i)
		mInputState.SetPressure(i, 0);

	// Wait until Application says device is active
	if(!IsActive()) {
		RumbleClear();
		return;
	}

	mInputState.BeginCycle();



#define U8_TO_S8(v)		(S8)((v) >> 1)
#define S16_TO_S8(v)	(S8)((v) >> 8)
#define BOOL_TO_S8(v)	(S8)((v) ? 127 : 0)

	S8 ltrigger = 0, rtrigger = 0;
	if(gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD
			|| gamepad.bLeftTrigger < -XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
		ltrigger = U8_TO_S8(gamepad.bLeftTrigger);
	if(gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD
			|| gamepad.bRightTrigger < -XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
		rtrigger = U8_TO_S8(gamepad.bRightTrigger);

	//orkprintf("ltrigger<%d> rtrigger<%d>\n", int(ltrigger), int(rtrigger));

	mInputState.SetPressure(ETRIG_RAW_JOY0_L1, ltrigger);
	mInputState.SetPressure(ETRIG_RAW_JOY0_R1, rtrigger);

	S8 lthumbX = 0, lthumbY = 0, rthumbX = 0, rthumbY = 0;
	if(gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
			|| gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		lthumbX = S16_TO_S8(gamepad.sThumbLX);
	if(gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
			|| gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		lthumbY = S16_TO_S8(gamepad.sThumbLY);
	if(gamepad.sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
			|| gamepad.sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
		rthumbX = S16_TO_S8(gamepad.sThumbRX);
	if(gamepad.sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
			|| gamepad.sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
		rthumbY = S16_TO_S8(gamepad.sThumbRY);

	//orkprintf("lthumbX<%d> lthumbY<%d> rthumbX<%d> rthumbY<%d>\n", int(lthumbX), int(lthumbY), int(rthumbX), int(rthumbY));

	mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, lthumbX);
	mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, lthumbY);
	mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_XAXIS, rthumbX);
	mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_YAXIS, rthumbY);

	S8 rdown = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_A);
	S8 rright = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_B);
	S8 rleft = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_X);
	S8 rup = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_Y);

	//orkprintf("rleft<%d> rright<%d> rup<%d> rdown<%d>\n", int(rleft), int(rright), int(rup), int(rdown));

	mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_DOWN, rdown);
	mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_RIGHT, rright);
	mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_LEFT, rleft);
	mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_UP, rup);

	S8 back = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_BACK);
	S8 start = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_START);

	//orkprintf("back<%d> start<%d>\n", int(back), int(start));

	mInputState.SetPressure(ETRIG_RAW_JOY0_BACK, back);
	mInputState.SetPressure(ETRIG_RAW_JOY0_START, start);

	S8 ldown = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
	S8 lright = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
	S8 lleft = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
	S8 lup = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);

	//orkprintf("lleft<%d> lright<%d> lup<%d> ldown<%d>\n", int(lleft), int(lright), int(lup), int(ldown));

	mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_DOWN, ldown );
	mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_RIGHT, lright );
	mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_LEFT, lleft );
	mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_UP, lup );

	S8 lshoulder = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
	S8 rshoulder = BOOL_TO_S8(gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

	//orkprintf("lshoulder<%d> rshoulder<%d>\n", int(lshoulder), int(rshoulder));

	mInputState.SetPressure(ETRIG_RAW_JOY0_L2, lshoulder);
	mInputState.SetPressure(ETRIG_RAW_JOY0_R2, rshoulder);

 // Set the vibration motors
	if(m_Rumbletimer > 0)
	{
		m_Rumbletimer--;
		if(m_Rumbletimer == 0) {
			m_fLeftMotorSpeed = 0.0f;
			m_fRightMotorSpeed = 0.0f;
		}
	}
    if(mRumbleEnabled && mMasterRumbleEnabled )
    {
        // Only alter the motor values if they changed
        WORD wLeftMotorSpeed = WORD( m_fLeftMotorSpeed * 65535.0f );
        WORD wRightMotorSpeed = WORD( m_fRightMotorSpeed * 65535.0f );

#if defined(_XBOX)
        if( mVibration.wLeftMotorSpeed != wLeftMotorSpeed ||
            mVibration.wRightMotorSpeed != wRightMotorSpeed )
        {
            mVibration.wLeftMotorSpeed = wLeftMotorSpeed;
            mVibration.wRightMotorSpeed = wRightMotorSpeed;
            XInputSetState( mdwUserIndex, &mVibration );
        }
#endif
	}


	mInputState.EndCycle();
}

} }

#endif
