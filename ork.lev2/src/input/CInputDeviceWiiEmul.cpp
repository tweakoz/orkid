////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Wiimote for PC (for wii game tuning on PC)
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined( _WIN32 )

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/input/CInputDeviceWiiEmul.h>
#include <ork/lev2/input/CInputDeviceDX.h>

#include <wiim/wiimote.h>
#pragma comment( lib, "hid.lib" )
#pragma comment( lib, "setupapi.lib" )

using namespace std;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

void ConnectedCallback(Wiimote *pWiiMote,const char* msg)
{
	static orkset<Wiimote *> mywiimotes;
	static int Connected_WiiMotes = 0;

	orkset<Wiimote *>::const_iterator it = mywiimotes.find( pWiiMote );

	if( it == mywiimotes.end() )
	{
		CInputDevice *pref = new CInputDeviceWiiEmul(pWiiMote,Connected_WiiMotes);
		CInputManager::GetRef().AddDevice(pref);
		Connected_WiiMotes++;
		mywiimotes.insert( pWiiMote );
	}
}


CInputWiiEmul::CInputWiiEmul()
{

	mwiimotes = HIDDevice::ConnectToHIDDevices<Wiimote>();

	mNumWiiMotes = mwiimotes.size();
	if(mNumWiiMotes > 4)
		mNumWiiMotes = 4;

	int mConnected_WiiMotes = 0;
	for(int i = 0; i < mNumWiiMotes; i++)
	{
		bool test = mwiimotes[i].StartListening();       // Start listening to input reports
		mwiimotes[i].RequestMotionData();    // Request the remote start sending back motion data
		mwiimotes[i].ConnectedCallBack(ConnectedCallback);
		while(mwiimotes[i].m_received_calibration == 0 ) {};
	}

}




/*
CInputWiiEmul::AddDisconnectMotes()
{
		int num_devs = CInputManager::GetRef().GetNumDevices();

		if(
		for (unsigned int i = 0; i< mNumWiiMotes - mconnected_wiimotes;i++)
		{
			CInputDevice *pref = new CInputDeviceWiiEmul_Unconnected(&mwiimotes[i],connected_wiimotes);

		}

}


void CInputDeviceWiiEmul_Unconnected::Input_Poll()
{
	for(int i = 0; i < mNumWiiMotes; i++)
	{
		if(mwiimotes[i].m_received_calibration  == 1) {
			mwiimotes[i].RequestMotionData();    // Request the remote start sending back motion data
			if(mwiimotes[i].m_received_calibration  == 2) {
				CInputDevice *pref = new CInputDeviceWiiEmul(&mwiimotes[i],connected_wiimotes);

		}

	}
}
*/



CInputDeviceWiiEmul::CInputDeviceWiiEmul(Wiimote *pWiiMote,int wiinum)
	: m_led(0)
	, mWiiMote(pWiiMote)
	, m_wiinum(wiinum + 1)
{
}

void CInputDeviceWiiEmul::Input_Poll()
{
	mConnectionStatus = CONN_STATUS_ACTIVE;

	InputState &inpstate = RefInputState();

	inpstate.BeginCycle();
	for(int i = ETRIG_RAW_BEGIN; i <= ETRIG_RAW_END; ++i)
		RefInputState().SetPressure(i, 0);

	if(m_led == 0)
	{
		mWiiMote->StartListening();       // Start listening to input reports
		mWiiMote->RequestMotionData();    // Request the remote start sending back motion data
	}

	m_led = m_wiinum ;

	int iled2 = m_led;
	mWiiMote->SetLEDs(iled2 & 1, iled2 & 2, iled2 & 4, iled2 & 8);    // Set the LEDs to show we've connected
	//m_led++;
	Button a = mWiiMote->GetButton("A");
	Button z = mWiiMote->GetButton("NunZ");
	Button b = mWiiMote->GetButton("B");
	Button c = mWiiMote->GetButton("NunC") ;

	Button b1 = mWiiMote->GetButton("1");
	Button b2 = mWiiMote->GetButton("2");

	Button up = mWiiMote->GetButton("Up");
	Button down = mWiiMote->GetButton("Down");
	Button left = mWiiMote->GetButton("Left");
	Button plus = mWiiMote->GetButton("+");
	Button minus = mWiiMote->GetButton("-");
	Button right = mWiiMote->GetButton("Right");



	if(mWiiMote->m_nunchuckconnected) {
		S8 analogx = mWiiMote->m_nunchuck_analogx - 127;
		if(abs(analogx) < 16) analogx = 0;
		if(analogx > 100) analogx = 127;
		if(analogx < -100) analogx = -127;

		S8 analogy = mWiiMote->m_nunchuck_analogy - 127;
		if(abs(analogy) < 16) analogy = 0;
		if(analogy > 100) analogy = 127;
		if(analogy < -100) analogy = -127;



		inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, analogx);
		inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, -analogy);

	}
	else
	{
		z.SetState(false);
		c.SetState(false);
	}

	if(a.Pressed() || z.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_ALPHA_A, 127);
	if(b.Pressed() || c.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_ALPHA_B, 127);

	if(b2.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_ALPHA_A, 127);
	if(b1.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_ALPHA_B, 127);

	// note rotated mWiiMoteote dir pad
	if(right.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_JOY0_LDIG_UP, 127);
	if(left.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_JOY0_LDIG_DOWN, 127);
	if(down.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_JOY0_LDIG_RIGHT, 127);
	if(up.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_JOY0_LDIG_LEFT, 127);

	if(plus.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_KEY_PLUS, 127);
	if(minus.Pressed())
		RefInputState().SetPressure(ETRIG_RAW_KEY_MINUS, 127);

	MotionData p = mWiiMote->GetLastMotionData();

	if(p.x != 0)
		RefInputState().SetPressure(ETRIG_RAW_JOY0_MOTION_X, p.x);
	if(p.z != 0)
		RefInputState().SetPressure(ETRIG_RAW_JOY0_MOTION_Y, -p.z);
	if(p.y != 0)
		RefInputState().SetPressure(ETRIG_RAW_JOY0_MOTION_Z, -p.y);



	float frumble = RefInputState().GetPressure(ETRIG_RAW_JOY0_RUMBLE);
	mWiiMote->SetRumble(frumble > 0.5f);

	inpstate.EndCycle();
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
#endif
