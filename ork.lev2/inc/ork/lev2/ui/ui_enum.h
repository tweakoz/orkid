////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_LEV2_UI_ENUM_H_
#define _ORK_LEV2_UI_ENUM_H_

namespace ork { namespace lev2 {
	
///////////////////////////////////////////////////////////////////////////////

enum EUIHandled
{
	EUI_NOT_HANDLED = 0,
	EUI_HANDLED ,
};

///////////////////////////////////////////////////////////////////////////////

enum EUIWidgetState
{	//																					Keyboard	Mouse	Joystick

	EUI_WIDGET_OFF = 0,					// 'visually' off									yes		yes		yes
	EUI_WIDGET_HOVER ,					// focus has stayed here for a certain time			yes		yes		yes
	EUI_WIDGET_ENTER ,					// this widget just recieved focus					yes		yes		yes
	EUI_WIDGET_EXIT ,					// this widget just lost focus						yes		yes		yes
	EUI_WIDGET_DRAG ,					// mouse is dragging widget							no		yes		no
};	// 3 bits

///////////////////////////////////////////////////////////////////////////////

} }

#endif
