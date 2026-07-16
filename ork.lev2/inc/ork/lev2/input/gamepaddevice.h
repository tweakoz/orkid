////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

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
// macOS + any non-Linux: a stub that always reports disconnected (the real mac
// backend lands in a later slice; the seam is GamepadDevice::Impl, per-platform in
// the .cpp).
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

  // thread-safe snapshot of the current pad state (reader thread updates concurrently)
  GamepadState sample() const;

  ~GamepadDevice();

protected:
  GamepadDevice();

  struct Impl;                  // per-platform (linux joydev / non-linux stub)
  std::unique_ptr<Impl> _impl;
};

} // namespace ork::lev2
