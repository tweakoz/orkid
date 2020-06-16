////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

enum EUIWidgetState { //
                      //                                      Joystick Keyboard	Mouse

  EUI_WIDGET_OFF = 0, // 'visually' off									           yes		yes		yes
  EUI_WIDGET_HOVER,   // focus has stayed here for a certain time	 yes		yes		yes
  EUI_WIDGET_ENTER,   // this widget just recieved focus					 yes		yes		yes
  EUI_WIDGET_EXIT,    // this widget just lost focus						   yes		yes		yes
  EUI_WIDGET_DRAG,    // mouse is dragging widget							      no		yes		no
};                    // 3 bits

enum class EventCode {
  UNKNOWN = 0,
  SHOW,
  HIDE,
  PUSH,
  DOUBLECLICK,
  RELEASE,
  DRAG,
  MOVE,
  KEY,
  KEY_REPEAT,
  KEYUP,
  DRAW,
  MOUSEWHEEL,
  MULTITOUCH,
  TABLET_BRUSH,
  GOT_KEYFOCUS,
  LOST_KEYFOCUS,
  ACTION,
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
