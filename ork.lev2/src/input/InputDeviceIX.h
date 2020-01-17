////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/input/inputdevice.h>

///////////////////////////////////////////////////////////////////////////////
// this has got to be the oldest class still remaining,
//  I mean CClass* ?
//  thats like 2005 orkid shit
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

class InputDeviceIX : public InputDevice {
  friend class InputManager;

public:
  InputDeviceIX(void);
  static void ClassInit(CClass* pClass); // lol
  static std::string GetClassName(void) { return std::string("InputDeviceIX"); }
  ~InputDeviceIX();

  void poll(void) final;
  virtual void Input_Init(void);
  virtual void Input_Configure(void);

private:
  std::map<uint32_t, uint32_t> _ixinputmap;
};

}} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////
