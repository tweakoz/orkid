////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _WII_INPUTMANAGER_H
#define _WII_INPUTMANAGER_H
#include <revolution.h>
#include <revolution/wpad.h>
#include <revolution/kpad.h>

//#undef WPADStopMotor
//#define WPADStopMotor(a)

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/input/input.h>

namespace ork { namespace lev2
{


class CInputWii
{
	public:
	CInputWii();


} ;

class CInputDeviceWii : public CInputDevice
{
	friend class CInputManager;

	public:
    CInputDeviceWii(int channel);
    ~CInputDeviceWii();

	virtual void Input_Poll();

	KPADStatus GetKPADStatus() { return mkpad[0]; }
	orkmap<int,int> mInputMap;

	//WPADStatus GetWPADStatus() { return mwpad; }
	int m_channel;

	private:



	KPADStatus	mkpad[ KPAD_MAX_READ_BUFS ] ;
	//WPADStatus  mwpad;


	s32		mkpad_reads ;
	int mdisconnectframe;


};

} }

///////////////////////////////////////////////////////////////////////////////

#endif // _DUMMY_INPUTMANAGER_H
