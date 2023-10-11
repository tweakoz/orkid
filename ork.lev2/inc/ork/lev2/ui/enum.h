////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

enum class EventCode : crc_enum_t {
  CrcEnum(UNKNOWN),
  CrcEnum(SHOW),
  CrcEnum(HIDE),
  CrcEnum(PUSH),
  CrcEnum(DOUBLECLICK),
  CrcEnum(RELEASE),
  CrcEnum(BEGIN_DRAG),
  CrcEnum(DRAG),
  CrcEnum(END_DRAG),
  CrcEnum(MOVE),
  CrcEnum(KEY_DOWN),
  CrcEnum(KEY_REPEAT),
  CrcEnum(KEY_UP),
  CrcEnum(RESIZED),
  CrcEnum(DRAW),
  CrcEnum(MOUSEWHEEL),
  CrcEnum(MULTITOUCH),
  CrcEnum(TABLET_BRUSH),
  CrcEnum(GOT_KEYFOCUS),
  CrcEnum(LOST_KEYFOCUS),
  CrcEnum(MOUSE_ENTER),
  CrcEnum(MOUSE_LEAVE),
  CrcEnum(ACTION),
  CrcEnum(PASTE_TEXT),
  CrcEnum(MIDI_CONTROLLER),
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
