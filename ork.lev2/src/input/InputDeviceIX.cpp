////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/thread.h>
#include <ork/pch.h>

#include "InputDeviceIX.h"

#include <functional>
#include <ork/kernel/csystem.h>
#include <ork/lev2/glfw/ctx_glfw.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

static float lanay = 0.0f;
static float ranay = 0.0f;
int lpid = -1;
int rpid = -1;

InputDeviceIX::InputDeviceIX() // 
  : InputDevice() {

  _ixinputmap[ETRIG_RAW_JOY0_LANA_YAXIS]=ETRIG_RAW_JOY0_LANA_YAXIS;
  _ixinputmap[ETRIG_RAW_JOY0_RANA_YAXIS]=ETRIG_RAW_JOY0_RANA_YAXIS;

}

InputDeviceIX::~InputDeviceIX() {}

///////////////////////////////////////////////////////////////////////////////

void InputDeviceIX::Input_Init(void) { return; }

///////////////////////////////////////////////////////////////////////////////

void InputDeviceIX::poll() {
  // printf( "POLL IXID\n");
  mConnectionStatus = CONN_STATUS_ACTIVE;

  InputState& inpstate = RefInputState();

  inpstate.BeginCycle();

  for (const auto& item : _ixinputmap) {
    uint32_t k = item.first;
    uint32_t v = item.second;
    int ist = int(OldSchool::IsKeyDepressed(k)) * 127;
    // printf( "KEY<%d> ST<%d>\n", int(k), ist );
    inpstate.SetPressure(v, ist);
  }

  inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, lanay * 127.0f);
  inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_YAXIS, ranay * 127.0f);

  inpstate.EndCycle();

  return;
}

///////////////////////////////////////////////////////////////////////////////

void InputDeviceIX::Input_Configure() { return; }

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
