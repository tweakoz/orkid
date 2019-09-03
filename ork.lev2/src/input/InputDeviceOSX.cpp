////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined( _OSX )

#include "InputDeviceOSX.h"

float atanf( float in )
{
	return atan( in );
}

float acosf( float in )
{
	return cos( in );
}

///////////////////////////////////////////////////////////////////////////////

InputDeviceOSX::InputDeviceOSX()
	: InputDevice()
{
	orkprintf( "created OSX Input Device\n" );
}


InputDeviceOSX::~InputDeviceOSX()
{
}

///////////////////////////////////////////////////////////////////////////////

void InputDeviceOSX::Input_Init(void)
{

    return ;
}

void InputDeviceOSX::Input_poll()
{
	mConnectionStatus = CONN_STATUS_ACTIVE;

	static bool bInitInputMap = true;
	static map<int,int> InputMap;

	if( bInitInputMap )
	{
		bInitInputMap = false;
		for( int i='a'; i<='z'; i++ ) OldStlSchoolMapInsert( InputMap, i, i );
		for( int i='0'; i<='9'; i++ ) OldStlSchoolMapInsert( InputMap, i, i );
		for( int i=(FL_F+1); i<=(FL_F+12); i++ ) OldStlSchoolMapInsert( InputMap, i, i );
		OldStlSchoolMapInsert( InputMap, FL_Up, (int) ETRIG_RAW_KEY_UP );
		OldStlSchoolMapInsert( InputMap, FL_Down, (int) ETRIG_RAW_KEY_DOWN );
		OldStlSchoolMapInsert( InputMap, FL_Left, (int) ETRIG_RAW_KEY_LEFT );
		OldStlSchoolMapInsert( InputMap, FL_Right, (int) ETRIG_RAW_KEY_RIGHT );
		OldStlSchoolMapInsert( InputMap, (int) '[', (int) '[' );
		OldStlSchoolMapInsert( InputMap, (int) ']', (int) ']' );
	}

	bool bALT = Fl::event_alt();
	bool bCTRL = Fl::event_ctrl();
	bool bSHIFT = Fl::event_shift();

	InputManager::SetTrigger( ETRIG_RAW_KEY_LSHIFT, bSHIFT ? 127 : 0 );
	InputManager::SetTrigger( ETRIG_RAW_KEY_RSHIFT, bSHIFT ? 127 : 0 );

	InputManager::SetTrigger( ETRIG_RAW_KEY_LCTRL, bCTRL ? 127 : 0 );
	InputManager::SetTrigger( ETRIG_RAW_KEY_RCTRL, bCTRL ? 127 : 0 );

	InputManager::SetTrigger( ETRIG_RAW_KEY_LALT, bALT ? 127 : 0 );
	InputManager::SetTrigger( ETRIG_RAW_KEY_RALT, bALT ? 127 : 0 );

	for( map<int,int>::const_iterator it=InputMap.begin(); it!=InputMap.end(); it++ )
	{
		pair<int,int> Value = *it;
		int ikey = Value.first;
		int iout = Value.second;
		int ipressed = Fl::get_key( ikey );
		InputManager::SetTrigger( iout, (ipressed==0) ? 0 : 127 );
	}


	bool bLANAL = OldSchool::IsKeyDepressed( 'g', 0 );
	bool bLANAR = OldSchool::IsKeyDepressed( 'h', 0 );
	bool bLANAU = OldSchool::IsKeyDepressed( 'y', 0 );
	bool bLANAD = OldSchool::IsKeyDepressed( 'b', 0 );
	bool bRDIGL = OldSchool::IsKeyDepressed( ETRIG_RAW_KEY_LEFT, 0 );
	bool bRDIGR = OldSchool::IsKeyDepressed( ETRIG_RAW_KEY_RIGHT, 0 );
	bool bRDIGU = OldSchool::IsKeyDepressed( ETRIG_RAW_KEY_UP, 0 );
	bool bRDIGD = OldSchool::IsKeyDepressed( ETRIG_RAW_KEY_DOWN, 0 );

	//orkprintf( "rdig %d %d %d %d\n", bRDIGU, bRDIGD, bRDIGL, bRDIGR );
	int ilanax = bLANAL ? -127 : bLANAR ? 127 : 0;
	int ilanay = bLANAU ? -127 : bLANAD ? 127 : 0;

/*		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LANA_XAXIS, ilanax  );
		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LANA_YAXIS, ilanay );
		//InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RANA_XAXIS, (S8) (255-(js.lZ>>8))-128 );
		//InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RANA_YAXIS, (S8) (js.lRz>>8)-128 );

		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_LEFT, bRDIGL ? 127 : 0 );
		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_UP, bRDIGU ? 127 : 0 );
		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_DOWN, bRDIGD ? 127 : 0 );
		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_RIGHT, bRDIGR ? 127 : 0 );

//		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_L1, OldSchool::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
//		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_R1, OldSchool::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
///		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_L2, OldSchool::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
//		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_R2, OldSchool::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );

//		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_SELECT, OldSchool::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
//		InputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_START, OldSchool::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
*/

   return ;

}





void InputDeviceOSX::Input_Configure()
{

    return ;
}







#endif
