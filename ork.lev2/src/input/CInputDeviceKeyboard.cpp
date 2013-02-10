////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/input/CInputDeviceKeyboard.h>

namespace ork { namespace lev2 {

CInputDeviceKeyboard::CInputDeviceKeyboard()
{
	for(int i = 'A'; i <= 'Z'; i++) OrkSTXMultiMapInsert(mKeyboardInputMap, i, i);
	for(int i = '0'; i <= '9'; i++) OrkSTXMultiMapInsert(mKeyboardInputMap, i, i);

#if defined (_WIN32)
	for(int i = 0; i <= 9; i++) OrkSTXMultiMapInsert(mKeyboardInputMap, VK_NUMPAD0 + i, i + '0');

	OrkSTXMultiMapInsert(mKeyboardInputMap, 'W', (int)ETRIG_RAW_JOY0_LDIG_UP);
	OrkSTXMultiMapInsert(mKeyboardInputMap, 'A', (int)ETRIG_RAW_JOY0_LDIG_LEFT);
	OrkSTXMultiMapInsert(mKeyboardInputMap, 'S', (int)ETRIG_RAW_JOY0_LDIG_DOWN);
	OrkSTXMultiMapInsert(mKeyboardInputMap, 'D', (int)ETRIG_RAW_JOY0_LDIG_RIGHT);

	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_UP, (int)ETRIG_RAW_JOY0_RDIG_UP);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_DOWN, (int)ETRIG_RAW_JOY0_RDIG_DOWN);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_LEFT, (int)ETRIG_RAW_JOY0_RDIG_LEFT);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_RIGHT, (int)ETRIG_RAW_JOY0_RDIG_RIGHT);

	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_LSHIFT, (int)ETRIG_RAW_KEY_LSHIFT);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_RSHIFT, (int)ETRIG_RAW_KEY_RSHIFT);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_LMENU, (int)ETRIG_RAW_KEY_LALT);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_RMENU, (int)ETRIG_RAW_KEY_RALT);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_LCONTROL, (int)ETRIG_RAW_KEY_LCTRL);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_RCONTROL, (int)ETRIG_RAW_KEY_RCTRL);

	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_NEXT, (int)ETRIG_RAW_KEY_PAGE_UP);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_PRIOR, (int)ETRIG_RAW_KEY_PAGE_DOWN);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_HOME, (int)ETRIG_RAW_KEY_HOME);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_END, (int)ETRIG_RAW_KEY_END);

	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F1, (int)ETRIG_RAW_KEY_FN1);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F2, (int)ETRIG_RAW_KEY_FN2);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F3, (int)ETRIG_RAW_KEY_FN3);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F4, (int)ETRIG_RAW_KEY_FN4);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F5, (int)ETRIG_RAW_KEY_FN5);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F6, (int)ETRIG_RAW_KEY_FN6);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F7, (int)ETRIG_RAW_KEY_FN7);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F8, (int)ETRIG_RAW_KEY_FN8);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F9, (int)ETRIG_RAW_KEY_FN9);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F10, (int)ETRIG_RAW_KEY_FN10);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F11, (int)ETRIG_RAW_KEY_FN11);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_F12, (int)ETRIG_RAW_KEY_FN12);

	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_RETURN, (int)ETRIG_RAW_JOY0_START);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_ESCAPE, (int)ETRIG_RAW_JOY0_BACK);

	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_ADD, (int)ETRIG_RAW_KEY_PLUS);
	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_SUBTRACT, (int)ETRIG_RAW_KEY_MINUS);

	OrkSTXMultiMapInsert(mKeyboardInputMap, VK_CAPITAL, (int)ETRIG_RAW_KEY_CAPSLOCK);

#endif
}

void CInputDeviceKeyboard::Input_Poll()
{
	mConnectionStatus = CONN_STATUS_ACTIVE;

	InputState &inpstate = RefInputState();

	inpstate.BeginCycle();

	// clear them all :P
	for(int i = ETRIG_RAW_BEGIN; i <= ETRIG_RAW_END; ++i)
		inpstate.SetPressure(i, 0);

	///////////////////////////////////////////////////////
	// raw keyboard modifiers
#if defined( _WIN32 ) && ! defined( _XBOX )
	for(orkmultimap<int, int>::const_iterator it = mKeyboardInputMap.begin(); it != mKeyboardInputMap.end(); it++)
	{
		std::pair<int, int> Value = *it;
		int ikey = Value.first;
		int iout = Value.second;

		SHORT keystate = GetAsyncKeyState(ikey);
		bool bkey = (keystate & 0x8000) == 0x8000;

		if(bkey)
		{
			inpstate.SetPressure(iout, 127);
		}
	}
#endif
	inpstate.EndCycle();
}

} // lev2

} // ork
