////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _PS2_INPUTMANAGER_H
#define _PS2_INPUTMANAGER_H

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2
{

class CInputDevicePS2 : public CInputDevice
{
	friend class CInputManager;

	public:
    
	CInputDevicePS2(void);
	static void ClassInit( CClass *pClass );
	static string GetClassName( void ) { return string("CInputDevicePS2"); }
    ~CInputDevicePS2();

	virtual void Input_Poll(void);
    virtual void Input_Init(void);
	virtual void Input_Configure(void);

	protected:

	bool mbInitialize;

//    HANDLE						mhPadHandles[4];
  //  XINPUT_POLLING_PARAMETERS	mInputPollParams;
//    XINPUT_STATE				mPadStates[4];


};

} }

///////////////////////////////////////////////////////////////////////////////

#endif
