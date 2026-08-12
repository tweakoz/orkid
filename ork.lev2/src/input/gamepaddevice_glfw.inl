////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// macOS gamepad backend, on GLFW's gamepad API. Included by gamepaddevice.cpp under
// __APPLE__ — not a standalone TU.
//
// Why GLFW rather than IOKit HID: GLFW carries SDL_GameControllerDB, so IT owns the
// DualShock 4 mapping. That matters specifically because the DS4 reports a DIFFERENT
// HID layout over Bluetooth than over USB; on the raw-HID path that divergence would
// be ours to track, and here it never reaches us. macOS's HID parser turns the pad's
// descriptor into elements, GLFW reads elements by usage and keys the mapping on the
// SDL GUID, and the DB carries the variants for both transports.
//
// GLFW's joystick calls are documented main-thread-only, so acquisition happens in
// pumpMainThread() (driven from the GLFW event pump) and publishes a locked snapshot
// that sample() serves to the update thread. sample()'s contract is therefore
// unchanged from the linux backend: cheap, thread-safe, returns the latest state.
////////////////////////////////////////////////////////////////

// NOTE: this file is included from INSIDE namespace ork::lev2, so it must not include
// anything itself — a system header pulled in here would land in ork::lev2::std. The
// GLFW and <mutex> includes live at the top of gamepaddevice.cpp, outside the
// namespace, exactly as the linux backend's headers do.

namespace {

// A pad exposing no GLFW mapping still yields raw axes/buttons. Their meaning is
//  device-specific, so the raw path assumes the near-universal dual-analog order
//  rather than inventing per-name tables — and it says so on connect, because a
//  wrong-looking raw pad is fixed by supplying a mapping, not by patching this.
enum {
  RAW_AX_LX = 0,
  RAW_AX_LY = 1,
  RAW_AX_RX = 2,
  RAW_AX_RY = 3,
};

} // namespace

////////////////////////////////////////////////////////////////////////////////

struct GamepadDevice::Impl {

  mutable std::mutex _mtx;
  GamepadState _state;    // guarded by _mtx — published by the main thread, read by any
  uint64_t _sampleCalls = 0;

  // main-thread-only below this line
  int _jid          = -1;    // currently reported joystick, -1 = none
  bool _mappingsTried = false;
  bool _dbg           = false;
  bool _dbgInit       = false;

  //////////////////////////////////////////////////////////////////////////////

  bool debugEnabled() {
    if (not _dbgInit) {
      _dbg     = (getenv("ORKID_GAMEPAD_DEBUG") != nullptr);
      _dbgInit = true;
    }
    return _dbg;
  }

  //////////////////////////////////////////////////////////////////////////////
  // A pad whose GUID is absent from the bundled DB reports no gamepad mapping and
  //  glfwGetGamepadState refuses it. ORKID_GAMEPAD_MAPPING takes an SDL mapping line
  //  so that is recoverable in the field without a rebuild.
  //////////////////////////////////////////////////////////////////////////////

  void applyExtraMappings() {
    if (_mappingsTried)
      return;

    _mappingsTried = true;
    const char* extra = getenv("ORKID_GAMEPAD_MAPPING");
    if (not extra)
      return;

    int ok = glfwUpdateGamepadMappings(extra);
    printf("[GAMEPAD] ORKID_GAMEPAD_MAPPING applied<%d>\n", ok);
    fflush(stdout);
  }

  //////////////////////////////////////////////////////////////////////////////

  void onConnect(int jid) {
    const char* name = glfwGetJoystickName(jid);
    const char* guid = glfwGetJoystickGUID(jid);
    bool mapped      = (glfwJoystickIsGamepad(jid) == GLFW_TRUE);
    const char* gpn  = mapped ? glfwGetGamepadName(jid) : nullptr;
    // The GUID is the diagnostic that matters when a pad arrives unmapped: it is the
    //  key an SDL mapping line is written against.
    printf("[GAMEPAD] connected: name<%s> jid<%d> guid<%s> profile<%s>\n",
           name ? name : "<unknown>",
           jid,
           guid ? guid : "<none>",
           mapped ? (gpn ? gpn : "GLFW mapping") : "RAW — sticks only, set ORKID_GAMEPAD_MAPPING");
    fflush(stdout);
  }

  //////////////////////////////////////////////////////////////////////////////

  void publish(const GamepadState& st) {
    std::lock_guard<std::mutex> lk(_mtx);
    _state = st;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Lowest connected joystick wins, matching the linux backend's "first node found".
  //////////////////////////////////////////////////////////////////////////////

  int findJoystick() {
    for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++)
      if (glfwJoystickPresent(jid) == GLFW_TRUE)
        return jid;

    return -1;
  }

  //////////////////////////////////////////////////////////////////////////////

  void pump() {
    applyExtraMappings();

    int jid = findJoystick();

    if (jid != _jid) {
      if (jid >= 0)
        onConnect(jid);
      else {
        printf("[GAMEPAD] disconnected: jid<%d>\n", _jid);
        fflush(stdout);
      }
      _jid = jid;
    }

    if (jid < 0) {
      publish(GamepadState{}); // connected=false, all controls released
      return;
    }

    GamepadState st;
    st.connected = true;

    GLFWgamepadstate gs;
    if (glfwGetGamepadState(jid, &gs) == GLFW_TRUE) {
      st.lx      = gamepad_norm::stickFromUnit(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
      st.ly      = gamepad_norm::stickFromUnit(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
      st.rx      = gamepad_norm::stickFromUnit(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
      st.ry      = gamepad_norm::stickFromUnit(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
      st.l2      = gamepad_norm::triggerFromUnit(gs.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
      st.r2      = gamepad_norm::triggerFromUnit(gs.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
      st.buttons = gamepad_norm::bitsFromGlfwButtons(gs.buttons, sizeof(gs.buttons));
    } else {
      // Unmapped pad: sticks only. Buttons are deliberately left released rather than
      //  guessed — a wrong button is worse than a dead one, and the connect line said so.
      int naxes          = 0;
      const float* axes  = glfwGetJoystickAxes(jid, &naxes);
      if (axes) {
        if (naxes > RAW_AX_LX)
          st.lx = gamepad_norm::stickFromUnit(axes[RAW_AX_LX]);
        if (naxes > RAW_AX_LY)
          st.ly = gamepad_norm::stickFromUnit(axes[RAW_AX_LY]);
        if (naxes > RAW_AX_RX)
          st.rx = gamepad_norm::stickFromUnit(axes[RAW_AX_RX]);
        if (naxes > RAW_AX_RY)
          st.ry = gamepad_norm::stickFromUnit(axes[RAW_AX_RY]);
      }
    }

    if (debugEnabled()) {
      static uint32_t s_prevButtons = 0;
      if (st.buttons != s_prevButtons) {
        printf("[GAMEPAD] buttons=0x%04x\n", unsigned(st.buttons));
        fflush(stdout);
        s_prevButtons = st.buttons;
      }
    }

    publish(st);
  }
};

////////////////////////////////////////////////////////////////////////////////

GamepadState GamepadDevice::sample() const {
  std::lock_guard<std::mutex> lk(_impl->_mtx);
  _impl->_sampleCalls++;
  return _impl->_state;
}

////////////////////////////////////////////////////////////////////////////////
// Constructs the singleton on first call — correct here, because on this platform a
//  pad genuinely can appear. GLFW must already be initialized (this runs from the GLFW
//  event pump); glfwJoystickPresent on an uninitialized GLFW is a no-op error, so an
//  early call degrades to "no pad" rather than crashing.
////////////////////////////////////////////////////////////////////////////////

void GamepadDevice::pumpMainThread() {
  GamepadDevice::instance()->_impl->pump();
}
