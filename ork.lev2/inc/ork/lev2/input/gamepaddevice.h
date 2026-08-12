////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ork/util/crc.h> // CrcEnum / crc_enum_t (abstract, python-token-aligned button ids)
#include <ork/lev2/lev2_types.h>

// GamepadDevice — a fresh, cross-platform-shaped gamepad reader. Deliberately NOT
// built on InputDevice/InputManager (that polling stack is dead: its root
// OldSchool::IsKeyDepressed hard-returns false and nothing populates it). This is a
// standalone service, exposed exactly like the input subsystem's InputManager —
// a shared_ptr singleton via GamepadDevice::instance().
//
// Linux backend: a background thread doing blocking-poll reads of struct js_event
// from a joydev node (/dev/input/js*). It autodetects the controller family by the
// joydev name (JSIOCGNAME) — DualShock/DualSense (hid_playstation) vs Xbox (xpad) —
// and picks the matching button table; the axis layout is identical across both.
//
// macOS backend: GLFW's gamepad API, which owns the HID mapping (its bundled
// SDL_GameControllerDB carries the DualShock 4 entries for both USB and Bluetooth, so
// the DS4's differing report layouts never reach us). Unlike Linux this has no reader
// thread: GLFW's joystick calls are main-thread-only, so pumpMainThread() snapshots
// under a lock from the GLFW event pump and sample() serves that snapshot. Consequence
// worth knowing: on macOS the pad samples at FRAME cadence, so a stalled main loop
// stalls pad input — on Linux the reader thread is independent of the frame.
//
// Linux deliberately does NOT move onto GLFW: GLFW's linux joystick backend is evdev,
// whose node is typically root:input 0660 (unreadable) where joydev is world-readable;
// headless runs force GLFW_PLATFORM_NULL, which has no joystick support at all; and the
// DRM scanout path has no GLFW context.
//
// Any other platform: a stub that always reports disconnected. The seam is
// GamepadDevice::Impl, per-platform in the .cpp.
//
// The host does NOT interpret the pad: it forwards the analog axes + abstract button
// ids to the scene's PythonSystem (see the ecs player), and the python input script
// owns the pad->locomotion mapping (exactly as it owns the keymap for InputKey).

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////
// Abstract controller button ids. CrcEnum => each value is the crc of its name, so
// it round-trips to a python token (tokens.CROSS, tokens.DPAD_UP, ...) with no magic
// numbers. DPAD_* are synthesized from the hat axes at sample() time (joydev exposes
// the DS4 dpad as ABS_HAT0X/Y, not buttons).
////////////////////////////////////////////////////////////////////////////////

enum class GamepadButtonId : ::ork::crc_enum_t {
  CrcEnum(CROSS),
  CrcEnum(CIRCLE),
  CrcEnum(SQUARE),
  CrcEnum(TRIANGLE),
  CrcEnum(L1),
  CrcEnum(R1),
  CrcEnum(L3),
  CrcEnum(R3),
  CrcEnum(SHARE),
  CrcEnum(OPTIONS),
  CrcEnum(PS),
  CrcEnum(DPAD_UP),
  CrcEnum(DPAD_DOWN),
  CrcEnum(DPAD_LEFT),
  CrcEnum(DPAD_RIGHT),
};

// The canonical bit order for GamepadState::buttons — bit i corresponds to
// kGamepadButtonOrder[i]. Consumers that need a python token for a pressed button
// build a crcstring_ptr_t from the id: std::make_shared<CrcString>(uint64_t(id)).
static constexpr size_t kNumGamepadButtons = 15;
extern const GamepadButtonId kGamepadButtonOrder[kNumGamepadButtons];

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Raw-to-normalized conversions, factored out of the backends so they are reachable
// from a unit test with no device, no window and no GLFW init. Every backend routes
// its raw values through these, so the value contract in GamepadState is stated in
// exactly one place per source encoding. Defined platform-independently in the .cpp.
////////////////////////////////////////////////////////////////////////////////

namespace gamepad_norm {

// joydev/evdev int16: stick [-32767,32767] -> [-1,1], clamped.
float stickFromI16(int16_t v);
// joydev/evdev int16 trigger: rest -32767 -> 0, full +32767 -> 1, clamped.
float triggerFromI16(int16_t v);
// GLFW already normalizes sticks to [-1,1]; clamp, and snap near-zero to zero so a
//  drifting stick cannot hold the host's change-detect path hot every frame. This is
//  NOT a deadzone — the real deadzone belongs to the python input script.
float stickFromUnit(float v);
// GLFW reports triggers in [-1,1] with rest at -1 -> [0,1].
float triggerFromUnit(float v);
// dpad-as-hat-axes (joydev) -> DPAD_* bits.
uint32_t hatToButtonBits(int16_t hx, int16_t hy);
// GLFWgamepadstate::buttons (15 entries, GLFW's own order) -> GamepadState::buttons bits.
//  GLFW's dpad run is UP/RIGHT/DOWN/LEFT against our UP/DOWN/LEFT/RIGHT, so this is a
//  permutation and never a straight copy.
uint32_t bitsFromGlfwButtons(const unsigned char* buttons, size_t count);

} // namespace gamepad_norm

////////////////////////////////////////////////////////////////////////////////

struct GamepadState {
  float lx = 0.0f, ly = 0.0f; // left stick  [-1,1] (up = negative)
  float rx = 0.0f, ry = 0.0f; // right stick [-1,1] (up = negative)
  float l2 = 0.0f, r2 = 0.0f; // triggers    [ 0,1] (rest = 0)
  uint32_t buttons = 0;       // bitmask packed per kGamepadButtonOrder (incl. synthesized DPAD_*)
  bool connected   = false;

  bool buttonDown(GamepadButtonId id) const;
};

////////////////////////////////////////////////////////////////////////////////

struct GamepadDevice {
  static gamepaddevice_ptr_t instance();

  // Main-thread service tick for backends whose platform API is main-thread-only (macOS
  //  /GLFW). MUST be called from the thread that owns the windowing system, right after
  //  the event poll. No-op where the backend owns a reader thread (linux) or does not
  //  exist. Static, and a no-op arm must never touch instance(): a platform that has no
  //  pad support must not be made to construct the singleton just by pumping.
  static void pumpMainThread();

  // thread-safe snapshot of the current pad state (backend updates concurrently:
  //  a reader thread on linux, pumpMainThread() on macOS)
  GamepadState sample() const;

  ~GamepadDevice();

protected:
  GamepadDevice();

  struct Impl;                  // per-platform (linux joydev / non-linux stub)
  std::unique_ptr<Impl> _impl;
};

} // namespace ork::lev2
