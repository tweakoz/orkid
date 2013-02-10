////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _DUMMY_INPUTMANAGER_H
#define _DUMMY_INPUTMANAGER_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/input/input.h>

namespace ork { namespace lev2
{

class CInputDeviceDummy : public CInputDevice
{
	friend class CInputManager;

	public:
    CInputDeviceDummy(void);
	static void ClassInit( CClass *pClass );
	static std::string GetClassName( void ) { return std::string("CInputDeviceDummy"); }
    ~CInputDeviceDummy();

	private:
};

} }

///////////////////////////////////////////////////////////////////////////////

#endif // _DUMMY_INPUTMANAGER_H
