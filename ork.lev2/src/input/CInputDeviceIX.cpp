////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/input/input.h>
#include <ork/lev2/input/CInputDeviceIX.h>

#if defined( _IOS )
#elif defined( IX )
    #include <ork/lev2/qtui/qtui.h>
#elif defined(_LINUX)
    #include <GL/glx.h>
    #include <Qt/QX11Info>
    #define XK_LATIN1
    #define XK_MISCELLANY
    #include <X11/keysymdef.h>
#endif
namespace ork { namespace lev2
{

///////////////////////////////////////////////////////////////////////////////

CInputDeviceIX::CInputDeviceIX() 
	: CInputDevice()
{
	printf("CREATED IX INPUTDEVICE\n" );
	OrkSTXMapInsert( mInputMap, 'W', (int) ETRIG_RAW_JOY0_LDIG_UP );	
	OrkSTXMapInsert( mInputMap, 'A', (int) ETRIG_RAW_JOY0_LDIG_LEFT );	
	OrkSTXMapInsert( mInputMap, 'D', (int) ETRIG_RAW_KEY_RIGHT );	
	OrkSTXMapInsert( mInputMap, 'S', (int) ETRIG_RAW_KEY_DOWN );	
	OrkSTXMapInsert( mInputMap, ETRIG_RAW_KEY_LEFT, (int) ETRIG_RAW_KEY_LEFT );	
	OrkSTXMapInsert( mInputMap, ETRIG_RAW_KEY_UP, (int) ETRIG_RAW_KEY_UP );	
	OrkSTXMapInsert( mInputMap, ETRIG_RAW_KEY_RIGHT, (int) ETRIG_RAW_KEY_RIGHT );	
	OrkSTXMapInsert( mInputMap, ETRIG_RAW_KEY_DOWN, (int) ETRIG_RAW_KEY_DOWN );	
}


CInputDeviceIX::~CInputDeviceIX()
{
}

///////////////////////////////////////////////////////////////////////////////

void CInputDeviceIX::Input_Init(void)
{

    return ;
}

void CInputDeviceIX::Input_Poll()
{
	//printf( "POLL IXID\n");
	mConnectionStatus = CONN_STATUS_ACTIVE;

	InputState &inpstate = RefInputState();

	inpstate.BeginCycle();

	for( const auto& item : mInputMap )
	{
		char k = item.first;
		char v = item.second;
		int ist = int(CSystem::IsKeyDepressed(k))*127;
		//printf( "KEY<%d> ST<%d>\n", int(k), ist );
		inpstate.SetPressure( v, ist );
	}

	inpstate.EndCycle();

   return ;

}





void CInputDeviceIX::Input_Configure()
{

    return ;
}




} }

