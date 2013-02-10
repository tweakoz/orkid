////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _WII_EMUL_INPUTMANAGER_H
#define _WII_EMUL_INPUTMANAGER_H


#include <wiim/wiimote.h>



#ifdef __cplusplus

namespace ork { namespace lev2
{
class CInputWiiEmul
{
public:
	CInputWiiEmul();

private:
	int mNumWiiMotes;
	int mConnected_WiiMotes;
	std::vector<Wiimote> mwiimotes;
};

class CInputDeviceWiiEmul : public ork::lev2::CInputDevice
{
	friend class CInputManager;

public:
    CInputDeviceWiiEmul(Wiimote *pw, int );
	static void ClassInit( CClass *pClass );
	static std::string GetClassName( void ) { return std::string("CInputDeviceWiiEmul"); }
    ~CInputDeviceWiiEmul();

	virtual void Input_Poll();

private:

	Wiimote *mWiiMote;

	int m_led;
	int m_wiinum;


};

} }

#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif // __cplusplus

///////////////////////////////////////////////////////////////////////////////

#endif // _WIN32_INPUTMANAGER_H
