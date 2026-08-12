////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Gamepad normalization + button-mapping tests. No device, no window, no GLFW init —
// every backend routes its raw values through ork::lev2::gamepad_norm, so the value
// contract is testable on any platform including the one whose pad is absent.
//
// What this is defending:
//  - the linux int16 conversions, so the macOS work could not silently reshape them
//  - the GLFW button permutation, which is where a mac-backend bug would actually live
//  - the CrcEnum token contract every python input script matches against
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <utpp/UnitTest++.h>

#include <ork/lev2/input/gamepaddevice.h>
#include <ork/util/crc.h>

#include <GLFW/glfw3.h>

using namespace ork;
using namespace ork::lev2;

namespace {
constexpr float kEps = 1e-5f;
}

////////////////////////////////////////////////////////////////////////////////
// int16 (joydev) sticks — the linux path, pinned so a refactor cannot drift it.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadStickFromI16) {
  CHECK_CLOSE(0.0f, gamepad_norm::stickFromI16(0), kEps);
  CHECK_CLOSE(1.0f, gamepad_norm::stickFromI16(32767), kEps);
  CHECK_CLOSE(-1.0f, gamepad_norm::stickFromI16(-32767), kEps);

  // int16 goes one lower than +32767; the clamp is what keeps the contract [-1,1].
  CHECK_CLOSE(-1.0f, gamepad_norm::stickFromI16(-32768), kEps);

  CHECK_CLOSE(0.5f, gamepad_norm::stickFromI16(16383), 1e-3f);
}

////////////////////////////////////////////////////////////////////////////////
// int16 triggers — rest is -32767 (NOT zero), the trap this conversion exists for.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadTriggerFromI16) {
  CHECK_CLOSE(0.0f, gamepad_norm::triggerFromI16(-32767), kEps);
  CHECK_CLOSE(1.0f, gamepad_norm::triggerFromI16(32767), kEps);
  CHECK_CLOSE(0.5f, gamepad_norm::triggerFromI16(0), 1e-3f);
  CHECK_CLOSE(0.0f, gamepad_norm::triggerFromI16(-32768), kEps);
}

////////////////////////////////////////////////////////////////////////////////
// GLFW unit-range conversions — the macOS path.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadStickFromUnit) {
  CHECK_CLOSE(0.0f, gamepad_norm::stickFromUnit(0.0f), kEps);
  CHECK_CLOSE(1.0f, gamepad_norm::stickFromUnit(1.0f), kEps);
  CHECK_CLOSE(-1.0f, gamepad_norm::stickFromUnit(-1.0f), kEps);
  CHECK_CLOSE(1.0f, gamepad_norm::stickFromUnit(2.5f), kEps);
  CHECK_CLOSE(-1.0f, gamepad_norm::stickFromUnit(-2.5f), kEps);
  CHECK_CLOSE(0.5f, gamepad_norm::stickFromUnit(0.5f), kEps);

  // near-zero snaps to exactly zero so stick drift cannot hold the host's
  //  change-detect path hot. Must stay far below the python deadzone (0.15).
  CHECK_CLOSE(0.0f, gamepad_norm::stickFromUnit(1e-4f), kEps);
  CHECK_CLOSE(0.0f, gamepad_norm::stickFromUnit(-1e-4f), kEps);
  CHECK(gamepad_norm::stickFromUnit(0.01f) != 0.0f);
}

TEST(GamepadTriggerFromUnit) {
  CHECK_CLOSE(0.0f, gamepad_norm::triggerFromUnit(-1.0f), kEps); // GLFW rest
  CHECK_CLOSE(1.0f, gamepad_norm::triggerFromUnit(1.0f), kEps);
  CHECK_CLOSE(0.5f, gamepad_norm::triggerFromUnit(0.0f), kEps);
  CHECK_CLOSE(0.0f, gamepad_norm::triggerFromUnit(-9.0f), kEps);
  CHECK_CLOSE(1.0f, gamepad_norm::triggerFromUnit(9.0f), kEps);
}

////////////////////////////////////////////////////////////////////////////////
// dpad synthesized from joydev hat axes.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadHatToButtonBits) {
  GamepadState st;

  st.buttons = gamepad_norm::hatToButtonBits(0, 0);
  CHECK_EQUAL(0u, st.buttons);

  st.buttons = gamepad_norm::hatToButtonBits(-32767, 0);
  CHECK(st.buttonDown(GamepadButtonId::DPAD_LEFT));
  CHECK(not st.buttonDown(GamepadButtonId::DPAD_RIGHT));

  st.buttons = gamepad_norm::hatToButtonBits(32767, 0);
  CHECK(st.buttonDown(GamepadButtonId::DPAD_RIGHT));

  st.buttons = gamepad_norm::hatToButtonBits(0, -32767);
  CHECK(st.buttonDown(GamepadButtonId::DPAD_UP));
  CHECK(not st.buttonDown(GamepadButtonId::DPAD_DOWN));

  st.buttons = gamepad_norm::hatToButtonBits(0, 32767);
  CHECK(st.buttonDown(GamepadButtonId::DPAD_DOWN));

  // diagonal: both axes latch
  st.buttons = gamepad_norm::hatToButtonBits(32767, -32767);
  CHECK(st.buttonDown(GamepadButtonId::DPAD_RIGHT));
  CHECK(st.buttonDown(GamepadButtonId::DPAD_UP));

  // under threshold -> nothing
  st.buttons = gamepad_norm::hatToButtonBits(15999, 15999);
  CHECK_EQUAL(0u, st.buttons);
}

////////////////////////////////////////////////////////////////////////////////
// GLFW button index -> abstract id. THE bug site for the macOS backend: GLFW orders
// its dpad UP/RIGHT/DOWN/LEFT and its thumbs after BACK/START/GUIDE, where we do not.
// Driven through the real GLFW_GAMEPAD_BUTTON_* constants so a GLFW reordering fails
// here rather than in someone's hands.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadBitsFromGlfwButtons) {
  struct Expect {
    int glfw_index;
    GamepadButtonId id;
  };
  const Expect kExpect[] = {
      {GLFW_GAMEPAD_BUTTON_A, GamepadButtonId::CROSS},
      {GLFW_GAMEPAD_BUTTON_B, GamepadButtonId::CIRCLE},
      {GLFW_GAMEPAD_BUTTON_X, GamepadButtonId::SQUARE},
      {GLFW_GAMEPAD_BUTTON_Y, GamepadButtonId::TRIANGLE},
      {GLFW_GAMEPAD_BUTTON_LEFT_BUMPER, GamepadButtonId::L1},
      {GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER, GamepadButtonId::R1},
      {GLFW_GAMEPAD_BUTTON_BACK, GamepadButtonId::SHARE},
      {GLFW_GAMEPAD_BUTTON_START, GamepadButtonId::OPTIONS},
      {GLFW_GAMEPAD_BUTTON_GUIDE, GamepadButtonId::PS},
      {GLFW_GAMEPAD_BUTTON_LEFT_THUMB, GamepadButtonId::L3},
      {GLFW_GAMEPAD_BUTTON_RIGHT_THUMB, GamepadButtonId::R3},
      {GLFW_GAMEPAD_BUTTON_DPAD_UP, GamepadButtonId::DPAD_UP},
      {GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, GamepadButtonId::DPAD_RIGHT},
      {GLFW_GAMEPAD_BUTTON_DPAD_DOWN, GamepadButtonId::DPAD_DOWN},
      {GLFW_GAMEPAD_BUTTON_DPAD_LEFT, GamepadButtonId::DPAD_LEFT},
  };

  // one button at a time: exactly the expected id, and nothing else
  for (const auto& e : kExpect) {
    unsigned char buttons[15] = {0};
    buttons[e.glfw_index]     = 1;

    GamepadState st;
    st.buttons = gamepad_norm::bitsFromGlfwButtons(buttons, 15);

    CHECK(st.buttonDown(e.id));

    int down = 0;
    for (size_t i = 0; i < kNumGamepadButtons; i++)
      if (st.buttonDown(kGamepadButtonOrder[i]))
        down++;
    CHECK_EQUAL(1, down);
  }

  // none pressed
  {
    unsigned char buttons[15] = {0};
    CHECK_EQUAL(0u, gamepad_norm::bitsFromGlfwButtons(buttons, 15));
  }

  // all pressed -> every abstract id down
  {
    unsigned char buttons[15];
    for (auto& b : buttons)
      b = 1;

    GamepadState st;
    st.buttons = gamepad_norm::bitsFromGlfwButtons(buttons, 15);
    for (size_t i = 0; i < kNumGamepadButtons; i++)
      CHECK(st.buttonDown(kGamepadButtonOrder[i]));
  }

  // defensive: null and over-long counts must not read out of bounds
  CHECK_EQUAL(0u, gamepad_norm::bitsFromGlfwButtons(nullptr, 15));
  {
    unsigned char buttons[15] = {0};
    buttons[GLFW_GAMEPAD_BUTTON_A] = 1;
    GamepadState st;
    st.buttons = gamepad_norm::bitsFromGlfwButtons(buttons, 999);
    CHECK(st.buttonDown(GamepadButtonId::CROSS));
  }
}

////////////////////////////////////////////////////////////////////////////////
// bit order <-> id table agreement. buttonDown() resolves an id by scanning
// kGamepadButtonOrder, so bit i must mean kGamepadButtonOrder[i] and nothing else.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadButtonOrderLockstep) {
  for (size_t i = 0; i < kNumGamepadButtons; i++) {
    GamepadState st;
    st.buttons = (1u << i);

    CHECK(st.buttonDown(kGamepadButtonOrder[i]));

    for (size_t j = 0; j < kNumGamepadButtons; j++)
      if (j != i)
        CHECK(not st.buttonDown(kGamepadButtonOrder[j]));
  }

  // ids are distinct — a duplicated CrcEnum would make two bits indistinguishable
  for (size_t i = 0; i < kNumGamepadButtons; i++)
    for (size_t j = i + 1; j < kNumGamepadButtons; j++)
      CHECK(kGamepadButtonOrder[i] != kGamepadButtonOrder[j]);
}

////////////////////////////////////////////////////////////////////////////////
// The python contract: each id is the crc of its own NAME, which is what lets a scene
// script match tokens.CROSS with no magic numbers. If this drifts, every python input
// script silently stops matching and nothing else fails first.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadButtonIdCrcTokens) {
  struct Token {
    GamepadButtonId id;
    const char* name;
  };
  const Token kTokens[] = {
      {GamepadButtonId::CROSS, "CROSS"},
      {GamepadButtonId::CIRCLE, "CIRCLE"},
      {GamepadButtonId::SQUARE, "SQUARE"},
      {GamepadButtonId::TRIANGLE, "TRIANGLE"},
      {GamepadButtonId::L1, "L1"},
      {GamepadButtonId::R1, "R1"},
      {GamepadButtonId::L3, "L3"},
      {GamepadButtonId::R3, "R3"},
      {GamepadButtonId::SHARE, "SHARE"},
      {GamepadButtonId::OPTIONS, "OPTIONS"},
      {GamepadButtonId::PS, "PS"},
      {GamepadButtonId::DPAD_UP, "DPAD_UP"},
      {GamepadButtonId::DPAD_DOWN, "DPAD_DOWN"},
      {GamepadButtonId::DPAD_LEFT, "DPAD_LEFT"},
      {GamepadButtonId::DPAD_RIGHT, "DPAD_RIGHT"},
  };

  // every id in the canonical order is covered here — a new button must land in both
  CHECK_EQUAL(kNumGamepadButtons, sizeof(kTokens) / sizeof(kTokens[0]));

  for (const auto& t : kTokens)
    CHECK_EQUAL(CrcString(t.name).hashed(), uint64_t(t.id));
}

////////////////////////////////////////////////////////////////////////////////
// A default-constructed state is the disconnected state every backend returns when no
// pad is present: nothing held, no axis deflection.
////////////////////////////////////////////////////////////////////////////////

TEST(GamepadDefaultStateIsNeutral) {
  GamepadState st;
  CHECK(not st.connected);
  CHECK_EQUAL(0u, st.buttons);
  CHECK_CLOSE(0.0f, st.lx, kEps);
  CHECK_CLOSE(0.0f, st.ly, kEps);
  CHECK_CLOSE(0.0f, st.rx, kEps);
  CHECK_CLOSE(0.0f, st.ry, kEps);
  CHECK_CLOSE(0.0f, st.l2, kEps); // triggers rest at 0, not -1
  CHECK_CLOSE(0.0f, st.r2, kEps);

  for (size_t i = 0; i < kNumGamepadButtons; i++)
    CHECK(not st.buttonDown(kGamepadButtonOrder[i]));
}
