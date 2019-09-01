////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/input/inputdevice.h>

///////////////////////////////////////////////////////////////////////////////

//#include <Fl/Fl.h>

namespace ork { namespace lev2
{

class InputDeviceIX : public InputDevice
{
	friend class InputManager;

	public:
    InputDeviceIX(void);
	static void ClassInit( CClass *pClass );
	static std::string GetClassName( void ) { return std::string("InputDeviceIX"); }
    ~InputDeviceIX();

	virtual void Input_Poll(void);
    virtual void Input_Init(void);
	virtual void Input_Configure(void);

	private:

	std::map<char,char> mInputMap;
};

} }

///////////////////////////////////////////////////////////////////////////////
