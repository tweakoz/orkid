////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/input/gamepaddevice.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__linux__)
#include <atomic>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/joystick.h>
#include <mutex>
#include <poll.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#endif

#if defined(__APPLE__)
// Backend headers belong OUT here, not in gamepaddevice_glfw.inl: that file is included
//  from inside namespace ork::lev2, where a system include would nest std:: under it.
#include <GLFW/glfw3.h>
#include <mutex>
#endif

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////
// GamepadState::buttons bit positions. Bit i == kGamepadButtonOrder[i]; keep the two
// in lockstep. (Platform-independent: the host reads these on every platform.)
////////////////////////////////////////////////////////////////////////////////

enum {
  BIT_CROSS      = 0,
  BIT_CIRCLE     = 1,
  BIT_SQUARE     = 2,
  BIT_TRIANGLE   = 3,
  BIT_L1         = 4,
  BIT_R1         = 5,
  BIT_L3         = 6,
  BIT_R3         = 7,
  BIT_SHARE      = 8,
  BIT_OPTIONS    = 9,
  BIT_PS         = 10,
  BIT_DPAD_UP    = 11,
  BIT_DPAD_DOWN  = 12,
  BIT_DPAD_LEFT  = 13,
  BIT_DPAD_RIGHT = 14,
};

const GamepadButtonId kGamepadButtonOrder[kNumGamepadButtons] = {
    GamepadButtonId::CROSS,     // 0
    GamepadButtonId::CIRCLE,    // 1
    GamepadButtonId::SQUARE,    // 2
    GamepadButtonId::TRIANGLE,  // 3
    GamepadButtonId::L1,        // 4
    GamepadButtonId::R1,        // 5
    GamepadButtonId::L3,        // 6
    GamepadButtonId::R3,        // 7
    GamepadButtonId::SHARE,     // 8
    GamepadButtonId::OPTIONS,   // 9
    GamepadButtonId::PS,        // 10
    GamepadButtonId::DPAD_UP,   // 11
    GamepadButtonId::DPAD_DOWN, // 12
    GamepadButtonId::DPAD_LEFT, // 13
    GamepadButtonId::DPAD_RIGHT // 14
};

bool GamepadState::buttonDown(GamepadButtonId id) const {
  for (size_t i = 0; i < kNumGamepadButtons; i++)
    if (kGamepadButtonOrder[i] == id)
      return (buttons & (1u << i)) != 0u;
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// GLFW gamepad BUTTON index -> GamepadState bit. Indices are fixed by the GLFW API
// (GLFW_GAMEPAD_BUTTON_*), so this table is spelled out numerically and stays free of
// the GLFW headers — which keeps it, and its test, building on every platform.
//
// The two orders agree for the first six entries and then diverge twice: GLFW runs
// BACK/START/GUIDE before the thumbs where we run the thumbs first, and GLFW's dpad is
// UP/RIGHT/DOWN/LEFT against our UP/DOWN/LEFT/RIGHT.
////////////////////////////////////////////////////////////////////////////////

static const int kGlfwBtnToBit[kNumGamepadButtons] = {
    BIT_CROSS,      // 0  GLFW_GAMEPAD_BUTTON_A            (DS4 cross)
    BIT_CIRCLE,     // 1  GLFW_GAMEPAD_BUTTON_B            (DS4 circle)
    BIT_SQUARE,     // 2  GLFW_GAMEPAD_BUTTON_X            (DS4 square)
    BIT_TRIANGLE,   // 3  GLFW_GAMEPAD_BUTTON_Y            (DS4 triangle)
    BIT_L1,         // 4  GLFW_GAMEPAD_BUTTON_LEFT_BUMPER
    BIT_R1,         // 5  GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER
    BIT_SHARE,      // 6  GLFW_GAMEPAD_BUTTON_BACK
    BIT_OPTIONS,    // 7  GLFW_GAMEPAD_BUTTON_START
    BIT_PS,         // 8  GLFW_GAMEPAD_BUTTON_GUIDE
    BIT_L3,         // 9  GLFW_GAMEPAD_BUTTON_LEFT_THUMB
    BIT_R3,         // 10 GLFW_GAMEPAD_BUTTON_RIGHT_THUMB
    BIT_DPAD_UP,    // 11 GLFW_GAMEPAD_BUTTON_DPAD_UP
    BIT_DPAD_RIGHT, // 12 GLFW_GAMEPAD_BUTTON_DPAD_RIGHT
    BIT_DPAD_DOWN,  // 13 GLFW_GAMEPAD_BUTTON_DPAD_DOWN
    BIT_DPAD_LEFT,  // 14 GLFW_GAMEPAD_BUTTON_DPAD_LEFT
};

////////////////////////////////////////////////////////////////////////////////

namespace gamepad_norm {

static inline float clampUnit(float f) {
  return f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f);
}

float stickFromI16(int16_t v) {
  return clampUnit(float(v) / 32767.0f);
}

float triggerFromI16(int16_t v) { // rest = -32767 -> 0, full = +32767 -> 1
  float f = (float(v) + 32767.0f) / 65534.0f;
  return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

float stickFromUnit(float v) {
  float f = clampUnit(v);
  return (f > -1e-3f and f < 1e-3f) ? 0.0f : f;
}

float triggerFromUnit(float v) { // rest = -1 -> 0, full = +1 -> 1
  float f = (clampUnit(v) + 1.0f) * 0.5f;
  return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

uint32_t hatToButtonBits(int16_t hx, int16_t hy) {
  uint32_t bits = 0;
  if (hx < -16000)
    bits |= (1u << BIT_DPAD_LEFT);
  else if (hx > 16000)
    bits |= (1u << BIT_DPAD_RIGHT);

  if (hy < -16000)
    bits |= (1u << BIT_DPAD_UP);
  else if (hy > 16000)
    bits |= (1u << BIT_DPAD_DOWN);

  return bits;
}

uint32_t bitsFromGlfwButtons(const unsigned char* buttons, size_t count) {
  uint32_t bits = 0;
  if (not buttons)
    return bits;

  if (count > kNumGamepadButtons)
    count = kNumGamepadButtons;

  for (size_t i = 0; i < count; i++)
    if (buttons[i])
      bits |= (1u << kGlfwBtnToBit[i]);

  return bits;
}

} // namespace gamepad_norm

////////////////////////////////////////////////////////////////////////////////

#if defined(__linux__)

////////////////////////////////////////////////////////////////////////////////
// joydev layout, autodetected by JSIOCGNAME (joydev carries no vendor/product id).
//
// joydev enumerates AXES by ascending ABS code and BUTTONS by ascending BTN code.
// The AXIS layout is IDENTICAL for DualShock 4 (hid_playstation) and Xbox (xpad);
// only the BUTTON order differs (xpad has no digital-trigger buttons, so View/Menu/
// Guide/L3/R3 shift down two indices). If real hardware disagrees, set
// ORKID_GAMEPAD_DEBUG=1 and wiggle each control: the first raw value seen per
// axis/button index is logged, so the tables can be corrected in minutes.
//
//   AXES (JSIOCGAXES == 8), both families:
//     0  ABS_X      left  stick X
//     1  ABS_Y      left  stick Y   (up = negative)
//     2  ABS_Z      L2/LT analog    (rest = -32767)
//     3  ABS_RX     right stick X
//     4  ABS_RY     right stick Y   (up = negative)
//     5  ABS_RZ     R2/RT analog    (rest = -32767)
//     6  ABS_HAT0X  dpad X          (-32767 left,  +32767 right)
//     7  ABS_HAT0Y  dpad Y          (-32767 up,    +32767 down)
////////////////////////////////////////////////////////////////////////////////

enum {
  AX_LX    = 0,
  AX_LY    = 1,
  AX_L2    = 2,
  AX_RX    = 3,
  AX_RY    = 4,
  AX_R2    = 5,
  AX_DPADX = 6,
  AX_DPADY = 7,
};

// DualShock 4 / DualSense (hid_playstation): joydev BUTTON index (JSIOCGBUTTONS == 13)
// -> GamepadState bit position (-1 = ignore).
static const int kPlayStationBtnToBit[] = {
    BIT_CROSS,    // 0  BTN_SOUTH
    BIT_CIRCLE,   // 1  BTN_EAST
    BIT_TRIANGLE, // 2  BTN_NORTH
    BIT_SQUARE,   // 3  BTN_WEST
    BIT_L1,       // 4  BTN_TL
    BIT_R1,       // 5  BTN_TR
    -1,           // 6  BTN_TL2  (L2 digital; analog via AX_L2)
    -1,           // 7  BTN_TR2  (R2 digital; analog via AX_R2)
    BIT_SHARE,    // 8  BTN_SELECT
    BIT_OPTIONS,  // 9  BTN_START
    BIT_PS,       // 10 BTN_MODE
    BIT_L3,       // 11 BTN_THUMBL
    BIT_R3,       // 12 BTN_THUMBR
};

// Xbox One / Series over USB (xpad): joydev BUTTON index (JSIOCGBUTTONS == 11) ->
// GamepadState bit position. No BTN_TL2/BTN_TR2 (triggers are analog only), so the
// SELECT/START/MODE/THUMBL/THUMBR run shifts down two vs the PlayStation table. The
// abstract ids are position-based: A/B/X/Y map onto CROSS/CIRCLE/SQUARE/TRIANGLE.
static const int kXboxBtnToBit[] = {
    BIT_CROSS,    // 0  BTN_SOUTH  = A
    BIT_CIRCLE,   // 1  BTN_EAST   = B
    BIT_TRIANGLE, // 2  BTN_NORTH  = Y
    BIT_SQUARE,   // 3  BTN_WEST   = X
    BIT_L1,       // 4  BTN_TL     = LB
    BIT_R1,       // 5  BTN_TR     = RB
    BIT_SHARE,    // 6  BTN_SELECT = View/Back
    BIT_OPTIONS,  // 7  BTN_START  = Menu
    BIT_PS,       // 8  BTN_MODE   = Guide
    BIT_L3,       // 9  BTN_THUMBL = L3
    BIT_R3,       // 10 BTN_THUMBR = R3
};

// controller family autodetection from the joydev name (case-insensitive substring).
// Xbox pads (xpad) name as "Microsoft X-Box One pad" / "Xbox Wireless Controller";
// everything else defaults to the PlayStation/generic table (DS4 target of record).
struct GamepadProfile {
  const char* name;
  const int* btnToBit;
  size_t btnCount;
};

static GamepadProfile detectProfile(const char* devname) {
  std::string lower(devname ? devname : "");
  for (auto& ch : lower)
    ch = char(::tolower((unsigned char)ch));
  bool is_xbox = (lower.find("xbox") != std::string::npos) or (lower.find("x-box") != std::string::npos);
  if (is_xbox)
    return {"Xbox", kXboxBtnToBit, sizeof(kXboxBtnToBit) / sizeof(kXboxBtnToBit[0])};
  return {"PlayStation", kPlayStationBtnToBit, sizeof(kPlayStationBtnToBit) / sizeof(kPlayStationBtnToBit[0])};
}

////////////////////////////////////////////////////////////////////////////////

struct GamepadDevice::Impl {

  static constexpr int kMaxAxis = 16;

  mutable std::mutex _mtx;
  int16_t _rawAxis[kMaxAxis] = {0};
  uint32_t _buttonBits       = 0;
  bool _connected            = false;

  const int* _btnToBit = kPlayStationBtnToBit; // active button table (per detected family)
  size_t _btnToBitCount = sizeof(kPlayStationBtnToBit) / sizeof(kPlayStationBtnToBit[0]);

  uint32_t _dbgSeenAxis = 0;
  uint32_t _dbgSeenBtn  = 0;

  // STAGE-1 liveness (device): counts joydev events processed and sample() calls per
  // ~5s window. Prints only while CONNECTED, only when a count is nonzero OR it just
  // transitioned to zero (the transition line = the device/player-thread died). Silent
  // with no pad (never connected -> no thread activity here).
  uint64_t _rxEvents    = 0; // guarded by _mtx
  uint64_t _sampleCalls = 0; // guarded by _mtx
  std::chrono::steady_clock::time_point _liveT0;
  bool _rxWasNz  = false; // reader-thread-only
  bool _smpWasNz = false; // reader-thread-only

  std::atomic<bool> _running{true};
  std::thread _thread;

  Impl() {
    _liveT0 = std::chrono::steady_clock::now();
    _thread = std::thread([this]() { readerLoop(); });
  }
  ~Impl() {
    _running.store(false);
    if (_thread.joinable())
      _thread.join();
  }

  //////////////////////////////////////////////////////////////////////////////

  void sleepChunks(int ms) { // interruptible sleep so shutdown is prompt
    int elapsed = 0;
    while (_running.load() and elapsed < ms) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      elapsed += 100;
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  void maybePrintLiveness() { // STAGE-1 device liveness (throttled ~5s), reader thread
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - _liveT0).count() < 5.0)
      return;
    _liveT0 = now;
    uint64_t rx, smp;
    bool connected;
    {
      std::lock_guard<std::mutex> lk(_mtx);
      rx           = _rxEvents;
      smp          = _sampleCalls;
      _rxEvents    = 0;
      _sampleCalls = 0;
      connected    = _connected;
    }
    if (not connected) { // no live pad -> stay silent (neutrality)
      _rxWasNz  = false;
      _smpWasNz = false;
      return;
    }
    if ((rx > 0) or (smp > 0) or _rxWasNz or _smpWasNz) {
      printf("[GAMEPAD] rx events=%llu sample=%llu /5s\n",
             (unsigned long long)rx, (unsigned long long)smp);
      fflush(stdout);
    }
    _rxWasNz  = (rx > 0);
    _smpWasNz = (smp > 0);
  }

  //////////////////////////////////////////////////////////////////////////////
  // prefer the stable by-id joystick symlink, else fall back to js0. accept any
  // joystick node (name logged on connect); vendor/product verification is not
  // available on the joydev interface (it carries no device id).
  //////////////////////////////////////////////////////////////////////////////

  int openDevice(std::string& node_out) {
    // Candidate list: by-id joystick symlinks EXCLUDING evdev nodes. A pad publishes BOTH
    //  "...-event-joystick" (evdev, struct input_event, usually root:input 660 — unreadable
    //  and the wrong protocol) and "...-joystick" (joydev, struct js_event, world-readable).
    //  The evdev name sorts FIRST alphabetically, so "take glob[0]" opened the wrong (and
    //  permission-denied) node and the reader spun silently. Skip "-event-", try every
    //  candidate, then fall back to raw js0..js3; log open failures once per node.
    std::vector<std::string> candidates;
    glob_t gl;
    memset(&gl, 0, sizeof(gl));
    if (glob("/dev/input/by-id/*-joystick", 0, nullptr, &gl) == 0) {
      for (size_t i = 0; i < gl.gl_pathc; i++) {
        std::string p = gl.gl_pathv[i];
        if (p.find("-event-") == std::string::npos)
          candidates.push_back(p);
      }
    }
    globfree(&gl);
    for (int j = 0; j < 4; j++)
      candidates.push_back(std::string("/dev/input/js") + char('0' + j));
    for (const auto& path : candidates) {
      int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
      if (fd >= 0) {
        node_out = path;
        return fd;
      }
      if (errno != ENOENT and _lastOpenFailNode != path) {
        _lastOpenFailNode = path;
        printf("[GAMEPAD] open failed: node<%s> errno<%d:%s> (will rescan)\n", path.c_str(), errno, strerror(errno));
        fflush(stdout);
      }
    }
    return -1;
  }
  std::string _lastOpenFailNode;

  //////////////////////////////////////////////////////////////////////////////

  void processEvent(const js_event& e, bool dbg) {
    uint8_t type = e.type & ~JS_EVENT_INIT;
    std::lock_guard<std::mutex> lk(_mtx);
    _rxEvents++;
    if (type == JS_EVENT_AXIS) {
      if (e.number < kMaxAxis)
        _rawAxis[e.number] = e.value;
      if (dbg and e.number < 32 and not(_dbgSeenAxis & (1u << e.number))) {
        _dbgSeenAxis |= (1u << e.number);
        printf("[GAMEPAD] axis[%u] raw=%d\n", unsigned(e.number), int(e.value));
        fflush(stdout);
      }
    } else if (type == JS_EVENT_BUTTON) {
      if (dbg and e.number < 32 and not(_dbgSeenBtn & (1u << e.number))) {
        _dbgSeenBtn |= (1u << e.number);
        printf("[GAMEPAD] button[%u] raw=%d\n", unsigned(e.number), int(e.value));
        fflush(stdout);
      }
      if (size_t(e.number) < _btnToBitCount) {
        int bit = _btnToBit[e.number];
        if (bit >= 0) {
          if (e.value)
            _buttonBits |= (1u << bit);
          else
            _buttonBits &= ~(1u << bit);
        }
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  void readerLoop() {
    bool dbg         = (getenv("ORKID_GAMEPAD_DEBUG") != nullptr);
    bool warned_disc = false;
    while (_running.load()) {
      std::string node;
      int fd = openDevice(node);
      if (fd < 0) {
        sleepChunks(2000); // slow rescan (minimal hotplug; full hardening is a later slice)
        continue;
      }
      char name[256]      = {0};
      unsigned char naxes = 0, nbtns = 0;
      if (ioctl(fd, JSIOCGNAME(sizeof(name) - 1), name) < 0)
        strncpy(name, "<unknown>", sizeof(name) - 1);
      ioctl(fd, JSIOCGAXES, &naxes);
      ioctl(fd, JSIOCGBUTTONS, &nbtns);
      GamepadProfile prof = detectProfile(name);
      printf("[GAMEPAD] connected: name<%s> node<%s> axes<%d> buttons<%d> profile<%s>\n",
             name, node.c_str(), int(naxes), int(nbtns), prof.name);
      fflush(stdout);
      warned_disc = false;
      {
        std::lock_guard<std::mutex> lk(_mtx);
        _connected     = true;
        _btnToBit      = prof.btnToBit;
        _btnToBitCount = prof.btnCount;
        _dbgSeenAxis   = 0;
        _dbgSeenBtn    = 0;
      }
      struct pollfd pfd;
      pfd.fd      = fd;
      pfd.events  = POLLIN;
      pfd.revents = 0;
      bool alive  = true;
      while (_running.load() and alive) {
        maybePrintLiveness(); // STAGE-1 liveness heartbeat (caught even while the pad is idle)
        int pr = poll(&pfd, 1, 200); // 200ms wake so shutdown/_running is checked promptly
        if (pr < 0) {
          if (errno == EINTR)
            continue;
          alive = false;
          break;
        }
        if (pr == 0)
          continue;
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
          alive = false;
          break;
        }
        if (pfd.revents & POLLIN) {
          js_event e;
          while (true) {
            ssize_t n = ::read(fd, &e, sizeof(e));
            if (n == (ssize_t)sizeof(e)) {
              processEvent(e, dbg);
              continue;
            }
            if (n < 0 and (errno == EAGAIN or errno == EWOULDBLOCK))
              break;      // drained
            alive = false; // EOF or hard error -> disconnect
            break;
          }
        }
      }
      ::close(fd);
      {
        std::lock_guard<std::mutex> lk(_mtx);
        _connected  = false;
        _buttonBits = 0;
        memset(_rawAxis, 0, sizeof(_rawAxis));
      }
      if (_running.load() and not warned_disc) {
        printf("[GAMEPAD] disconnected: node<%s> (rescanning)\n", node.c_str());
        fflush(stdout);
        warned_disc = true;
      }
      if (_running.load())
        sleepChunks(2000);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////

GamepadState GamepadDevice::sample() const {
  GamepadState st;
  std::lock_guard<std::mutex> lk(_impl->_mtx);
  _impl->_sampleCalls++; // STAGE-1 liveness: proves the player update thread keeps polling
  if (not _impl->_connected)
    return st; // default: connected=false
  st.lx = gamepad_norm::stickFromI16(_impl->_rawAxis[AX_LX]);
  st.ly = gamepad_norm::stickFromI16(_impl->_rawAxis[AX_LY]);
  st.l2 = gamepad_norm::triggerFromI16(_impl->_rawAxis[AX_L2]);
  st.rx = gamepad_norm::stickFromI16(_impl->_rawAxis[AX_RX]);
  st.ry = gamepad_norm::stickFromI16(_impl->_rawAxis[AX_RY]);
  st.r2 = gamepad_norm::triggerFromI16(_impl->_rawAxis[AX_R2]);
  st.buttons =
      _impl->_buttonBits | gamepad_norm::hatToButtonBits(_impl->_rawAxis[AX_DPADX], _impl->_rawAxis[AX_DPADY]);
  st.connected = true;
  return st;
}

////////////////////////////////////////////////////////////////////////////////
// The joydev reader thread owns acquisition; nothing to service on the main thread.
////////////////////////////////////////////////////////////////////////////////

void GamepadDevice::pumpMainThread() {
}

////////////////////////////////////////////////////////////////////////////////

#elif defined(__APPLE__)

#include "gamepaddevice_glfw.inl"

////////////////////////////////////////////////////////////////////////////////

#else // no backend for this platform — compiles clean, always disconnected.

struct GamepadDevice::Impl {};

GamepadState GamepadDevice::sample() const {
  return GamepadState{}; // connected = false
}

// Must not touch instance(): pumping is not a reason to construct a device that can
//  never report a pad.
void GamepadDevice::pumpMainThread() {
}

#endif

////////////////////////////////////////////////////////////////////////////////

GamepadDevice::GamepadDevice() {
  _impl = std::make_unique<Impl>();
}

GamepadDevice::~GamepadDevice() {
} // out-of-line: Impl is complete here (linux thread join / stub no-op)

////////////////////////////////////////////////////////////////////////////////

gamepaddevice_ptr_t GamepadDevice::instance() {
  struct concrete : public GamepadDevice {
    concrete()
        : GamepadDevice() {
    }
  };
  static gamepaddevice_ptr_t _inst = std::make_shared<concrete>();
  return _inst;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
