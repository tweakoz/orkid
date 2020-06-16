////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace ui {

///////////////////////////////////////////////////////////////////////////////

enum EUIWidgetState { //
                      //                                      Joystick Keyboard	Mouse

  EUI_WIDGET_OFF = 0, // 'visually' off									           yes		yes		yes
  EUI_WIDGET_HOVER,   // focus has stayed here for a certain time	 yes		yes		yes
  EUI_WIDGET_ENTER,   // this widget just recieved focus					 yes		yes		yes
  EUI_WIDGET_EXIT,    // this widget just lost focus						   yes		yes		yes
  EUI_WIDGET_DRAG,    // mouse is dragging widget							      no		yes		no
};                    // 3 bits

enum EUIEventCode {
  UIEV_UNKNOWN = 0,
  UIEV_SHOW,
  UIEV_HIDE,
  UIEV_PUSH,
  UIEV_DOUBLECLICK,
  UIEV_RELEASE,
  UIEV_DRAG,
  UIEV_MOVE,
  UIEV_KEY,
  UIEV_KEY_REPEAT,
  UIEV_KEYUP,
  UIEV_DRAW,
  UIEV_MOUSEWHEEL,
  UIEV_MULTITOUCH,
  UIEV_TABLET_BRUSH,
  UIEV_GOT_KEYFOCUS,
  UIEV_LOST_KEYFOCUS,
  UIEV_ACTION,
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
