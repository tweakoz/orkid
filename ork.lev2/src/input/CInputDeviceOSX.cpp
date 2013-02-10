////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined( _OSX )

#include <input/input.h>
#include <input/CInputDeviceOSX.h>
#include <Fl/Fl.h>

float atanf( float in )
{
	return atan( in );
}

float acosf( float in )
{
	return cos( in );
}

///////////////////////////////////////////////////////////////////////////////

CInputDeviceOSX::CInputDeviceOSX() 
	: CInputDevice()
{
	orkprintf( "created OSX Input Device\n" );
}


CInputDeviceOSX::~CInputDeviceOSX()
{
}

///////////////////////////////////////////////////////////////////////////////

void CInputDeviceOSX::Input_Init(void)
{

    return ;
}

void CInputDeviceOSX::Input_Poll()
{
	mConnectionStatus = CONN_STATUS_ACTIVE;

	static bool bInitInputMap = true;
	static map<int,int> InputMap;
	
	if( bInitInputMap )
	{
		bInitInputMap = false;
		for( int i='a'; i<='z'; i++ ) OrkSTXMapInsert( InputMap, i, i );
		for( int i='0'; i<='9'; i++ ) OrkSTXMapInsert( InputMap, i, i );
		for( int i=(FL_F+1); i<=(FL_F+12); i++ ) OrkSTXMapInsert( InputMap, i, i );
		OrkSTXMapInsert( InputMap, FL_Up, (int) ETRIG_RAW_KEY_UP );	
		OrkSTXMapInsert( InputMap, FL_Down, (int) ETRIG_RAW_KEY_DOWN );	
		OrkSTXMapInsert( InputMap, FL_Left, (int) ETRIG_RAW_KEY_LEFT );	
		OrkSTXMapInsert( InputMap, FL_Right, (int) ETRIG_RAW_KEY_RIGHT );	
		OrkSTXMapInsert( InputMap, (int) '[', (int) '[' );	
		OrkSTXMapInsert( InputMap, (int) ']', (int) ']' );	
	}
		
	bool bALT = Fl::event_alt();
	bool bCTRL = Fl::event_ctrl();
	bool bSHIFT = Fl::event_shift();
	
	CInputManager::SetTrigger( ETRIG_RAW_KEY_LSHIFT, bSHIFT ? 127 : 0 );
	CInputManager::SetTrigger( ETRIG_RAW_KEY_RSHIFT, bSHIFT ? 127 : 0 );
	
	CInputManager::SetTrigger( ETRIG_RAW_KEY_LCTRL, bCTRL ? 127 : 0 );
	CInputManager::SetTrigger( ETRIG_RAW_KEY_RCTRL, bCTRL ? 127 : 0 );
	
	CInputManager::SetTrigger( ETRIG_RAW_KEY_LALT, bALT ? 127 : 0 );
	CInputManager::SetTrigger( ETRIG_RAW_KEY_RALT, bALT ? 127 : 0 );

	for( map<int,int>::const_iterator it=InputMap.begin(); it!=InputMap.end(); it++ )
	{
		pair<int,int> Value = *it;
		int ikey = Value.first;
		int iout = Value.second;
		int ipressed = Fl::get_key( ikey );		
		CInputManager::SetTrigger( iout, (ipressed==0) ? 0 : 127 );
	}


	bool bLANAL = CSystem::IsKeyDepressed( 'g', 0 );
	bool bLANAR = CSystem::IsKeyDepressed( 'h', 0 );
	bool bLANAU = CSystem::IsKeyDepressed( 'y', 0 );
	bool bLANAD = CSystem::IsKeyDepressed( 'b', 0 );
	bool bRDIGL = CSystem::IsKeyDepressed( ETRIG_RAW_KEY_LEFT, 0 );
	bool bRDIGR = CSystem::IsKeyDepressed( ETRIG_RAW_KEY_RIGHT, 0 );
	bool bRDIGU = CSystem::IsKeyDepressed( ETRIG_RAW_KEY_UP, 0 );
	bool bRDIGD = CSystem::IsKeyDepressed( ETRIG_RAW_KEY_DOWN, 0 );

	//orkprintf( "rdig %d %d %d %d\n", bRDIGU, bRDIGD, bRDIGL, bRDIGR );
	int ilanax = bLANAL ? -127 : bLANAR ? 127 : 0;
	int ilanay = bLANAU ? -127 : bLANAD ? 127 : 0;

/*		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LANA_XAXIS, ilanax  );
		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LANA_YAXIS, ilanay );
		//CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RANA_XAXIS, (S8) (255-(js.lZ>>8))-128 );
		//CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RANA_YAXIS, (S8) (js.lRz>>8)-128 );

		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_LEFT, bRDIGL ? 127 : 0 );
		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_UP, bRDIGU ? 127 : 0 );
		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_DOWN, bRDIGD ? 127 : 0 );
		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_RIGHT, bRDIGR ? 127 : 0 );

//		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_L1, CSystem::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
//		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_R1, CSystem::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
///		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_L2, CSystem::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
//		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_R2, CSystem::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );

//		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_SELECT, CSystem::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
//		CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_START, CSystem::IsKeyDepressed( 'g', 0 ) ? 127 : 0 );
*/

   return ;

}





void CInputDeviceOSX::Input_Configure()
{

    return ;
}







#endif

