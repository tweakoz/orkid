////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <ork/kernel/core/singleton.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/svariant.h>
#include <unordered_map>

namespace ork { namespace lev2 {

enum ERawTriggerNames {
  ETRIG_RAW_DISCONNECT = -1,
  ETRIG_RAW_BEGIN = 0,

  ///////////////////////////////
  // Raw Values

  ETRIG_RAW_ALPHA_A = 'A',
  ETRIG_RAW_ALPHA_B = 'B',
  ETRIG_RAW_ALPHA_C = 'C',
  ETRIG_RAW_ALPHA_D = 'D',
  ETRIG_RAW_ALPHA_E = 'E',
  ETRIG_RAW_ALPHA_S = 'S',
  ETRIG_RAW_ALPHA_W = 'W',
  ETRIG_RAW_ALPHA_Z = 'Z',
  ETRIG_RAW_NUMBER_0 = '0',
  ETRIG_RAW_NUMBER_1 = '1',
  ETRIG_RAW_NUMBER_2 = '2',
  ETRIG_RAW_NUMBER_9 = '9',

  ETRIG_RAW_KEY_PLUS = '+',
  ETRIG_RAW_KEY_MINUS = '-',

  ETRIG_RAW_KEY_ALPHABASE = ETRIG_RAW_ALPHA_A,

  ETRIG_RAW_KEY_SPACEBAR = 32,

  ETRIG_RAW_KEY_FN1 = 112,
  ETRIG_RAW_KEY_FN2,
  ETRIG_RAW_KEY_FN3,
  ETRIG_RAW_KEY_FN4,
  ETRIG_RAW_KEY_FN5,
  ETRIG_RAW_KEY_FN6,
  ETRIG_RAW_KEY_FN7,
  ETRIG_RAW_KEY_FN8,
  ETRIG_RAW_KEY_FN9,
  ETRIG_RAW_KEY_FN10,
  ETRIG_RAW_KEY_FN11,
  ETRIG_RAW_KEY_FN12,

  ETRIG_RAW_KEY_CAPSLOCK,

  ETRIG_RAW_KEY_AUXBASE = 192,

  ETRIG_RAW_KEY_UP = 192,
  ETRIG_RAW_KEY_LEFT,
  ETRIG_RAW_KEY_RIGHT,
  ETRIG_RAW_KEY_DOWN,

  ETRIG_RAW_KEY_PAGE_UP,
  ETRIG_RAW_KEY_PAGE_DOWN,

  ETRIG_RAW_KEY_HOME,
  ETRIG_RAW_KEY_END,

  ETRIG_RAW_KEY_CAPS_LOCK,
  ETRIG_RAW_KEY_LSHIFT,
  ETRIG_RAW_KEY_RSHIFT,
  ETRIG_RAW_KEY_LCTRL,
  ETRIG_RAW_KEY_RCTRL,
  ETRIG_RAW_KEY_LALT,
  ETRIG_RAW_KEY_RALT,

  //
  ETRIG_RAW_JOY0 = 256,
  ETRIG_RAW_JOY0_LDIG_UP = 256,
  ETRIG_RAW_JOY0_LDIG_DOWN,
  ETRIG_RAW_JOY0_LDIG_LEFT,
  ETRIG_RAW_JOY0_LDIG_RIGHT,

  ETRIG_RAW_JOY0_RDIG_UP,
  ETRIG_RAW_JOY0_RDIG_DOWN,
  ETRIG_RAW_JOY0_RDIG_LEFT,
  ETRIG_RAW_JOY0_RDIG_RIGHT,

  ETRIG_RAW_JOY0_LANA_XAXIS,
  ETRIG_RAW_JOY0_LANA_YAXIS,

  ETRIG_RAW_JOY0_RANA_XAXIS,
  ETRIG_RAW_JOY0_RANA_YAXIS,

  ETRIG_RAW_JOY0_ANA_ZAXIS,

  ETRIG_RAW_JOY0_L1,
  ETRIG_RAW_JOY0_L2,
  ETRIG_RAW_JOY0_R1,
  ETRIG_RAW_JOY0_R2,

  ETRIG_RAW_JOY0_START,
  ETRIG_RAW_JOY0_SELECT,
  ETRIG_RAW_JOY0_BACK,

  ETRIG_RAW_JOY0_L3, // e joystick buttons
  ETRIG_RAW_JOY0_R3,

  ETRIG_RAW_JOY0_MOTION_X,
  ETRIG_RAW_JOY0_MOTION_Y,
  ETRIG_RAW_JOY0_MOTION_Z,

  ETRIG_RAW_JOY0_RUMBLE,

  ETRIG_RAW_JOY0_LANA_UP,
  ETRIG_RAW_JOY0_LANA_DOWN,
  ETRIG_RAW_JOY0_LANA_LEFT,
  ETRIG_RAW_JOY0_LANA_RIGHT,

  //
  ETRIG_RAW_JOY1 = 512,
  ETRIG_RAW_JOY1_LDIG_UP = 512,
  ETRIG_RAW_JOY1_LDIG_DOWN,
  ETRIG_RAW_JOY1_LDIG_LEFT,
  ETRIG_RAW_JOY1_LDIG_RIGHT,

  ETRIG_RAW_JOY1_RDIG_UP,
  ETRIG_RAW_JOY1_RDIG_DOWN,
  ETRIG_RAW_JOY1_RDIG_LEFT,
  ETRIG_RAW_JOY1_RDIG_RIGHT,

  ETRIG_RAW_JOY1_LANA_XAXIS,
  ETRIG_RAW_JOY1_LANA_YAXIS,

  ETRIG_RAW_JOY1_RANA_XAXIS,
  ETRIG_RAW_JOY1_RANA_YAXIS,

  ETRIG_RAW_JOY1_LANA_UP,
  ETRIG_RAW_JOY1_LANA_DOWN,
  ETRIG_RAW_JOY1_LANA_LEFT,
  ETRIG_RAW_JOY1_LANA_RIGHT,

  ETRIG_RAW_JOY1_L1,
  ETRIG_RAW_JOY1_L2,
  ETRIG_RAW_JOY1_R1,
  ETRIG_RAW_JOY1_R2,

  ETRIG_RAW_JOY1_START,
  ETRIG_RAW_JOY1_SELECT,

  ETRIG_RAW_JOY1_L3, // e joystick buttons
  ETRIG_RAW_JOY1_R3,

  ETRIG_RAW_END = 1023
};

enum EMappedTriggerNames {
  ETRIG_BEGIN = 256,

  ETRIG_CAMZOOM_IN,
  ETRIG_CAMZOOM_OUT,

  ETRIG_CAMEDIT_WORLD,
  ETRIG_CAMEDIT_TOGL1,
  ETRIG_CAMEDIT_TOGL2,
  ETRIG_CAMEDIT_TOGL3,
  ETRIG_CAMEDIT_MOVE_ZAXIS,
  ETRIG_CAMEDIT_MOVE_XAXIS,
  ETRIG_CAMEDIT_ROT_ZAXIS,
  ETRIG_CAMEDIT_ROT_XAXIS,
  ETRIG_CAMEDIT_ROT_YAXIS,

  ETRIG_PLAYER0_MOVE_FORWARD,
  ETRIG_PLAYER0_MOVE_BACKWARD,
  ETRIG_PLAYER0_MOVE_LEFT,
  ETRIG_PLAYER0_MOVE_RIGHT,
  ETRIG_PLAYER0_ROT_FORWARD,
  ETRIG_PLAYER0_ROT_BACKWARD,
  ETRIG_PLAYER0_ROT_LEFT,
  ETRIG_PLAYER0_ROT_RIGHT,

  ETRIG_PLAYER1_MOVE_FORWARD,
  ETRIG_PLAYER1_MOVE_BACKWARD,
  ETRIG_PLAYER1_MOVE_LEFT,
  ETRIG_PLAYER1_MOVE_RIGHT,
  ETRIG_PLAYER1_ROT_FORWARD,
  ETRIG_PLAYER1_ROT_BACKWARD,
  ETRIG_PLAYER1_ROT_LEFT,
  ETRIG_PLAYER1_ROT_RIGHT,
};

class InputManager;

static const int KMAX_TRIGGERS = 1024;

struct RawInputKey {
  uint32_t mKey;
  RawInputKey(uint32_t v = ETRIG_RAW_BEGIN) { mKey = v; }
  bool operator<(const RawInputKey& oth) const { return oth.mKey < mKey; }
};

struct MappedInputKey {
  uint32_t mKey;
  MappedInputKey(uint32_t v = ETRIG_BEGIN) { mKey = v; }
  bool operator<(const MappedInputKey& oth) const { return oth.mKey < mKey; }
};

class InputMap {
public:
  std::map<MappedInputKey, RawInputKey> mInputMap;
  RawInputKey MapInput(const MappedInputKey& inkey) const;
  void Set(MappedInputKey inch, RawInputKey outch);
};

class InputState {
  static InputMap gInputMap;

public:
  static InputMap& RefGlobalInputMap() { return gInputMap; }

  bool IsDown(MappedInputKey ch, const InputMap& InputMap = gInputMap) const;
  bool IsUpEdge(MappedInputKey ch, const InputMap& InputMap = gInputMap) const;
  bool IsDownEdge(MappedInputKey ch, const InputMap& InputMap = gInputMap) const;
  F32 GetPressure(MappedInputKey ch, const InputMap& InputMap = gInputMap) const;
  float GetPressureRaw(int ch) const;

  void SetPressure(RawInputKey ch, float uVal);
  void SetPressureRaw(int ich, float uVal);

  InputState();

  void BeginCycle();
  void EndCycle();
  void Clear(int index = -1);

private:
  float _prevPressureValues[KMAX_TRIGGERS];
  float _pressureValues[KMAX_TRIGGERS];
  float _pressureThresh[KMAX_TRIGGERS];

  bool _triggerDown[KMAX_TRIGGERS];
  bool _triggerUp[KMAX_TRIGGERS];
  bool _triggerState[KMAX_TRIGGERS];
};

///////////////////////////////////////////////////////////////////////////////

class InputDevice {
  friend class InputManager;

public:
  // Enum: CONN_STATUS
  // Connection status is intended to support all possible device states for all possible device.
  //
  // XInput makes it really easy to detect at runtime if the device has been plugged in or unplugged,
  // unlike DirectInput and its EnumDevices.
  //
  // Not all device types will support all of these states. For example, the keyboard may assume
  // that it is always connected (even if it has been unplugged).
  typedef enum {
    CONN_STATUS_UNKNOWN = -1,

    // Constant: CONN_STATUS_DISCONNECTED
    // Device is currently disconnected and has been for at least one call to Input_Poll
    CONN_STATUS_DISCONNECTED,
    // Constant: CONN_STATUS_REMOVED
    // Device was removed since last call to Input_Poll
    CONN_STATUS_REMOVED,

    // Constant: CONN_STATUS_CONNECTED
    // Device is currently connected and has been for at least one call to Input_Poll
    CONN_STATUS_CONNECTED,
    // Constant: CONN_STATUS_INSERTED
    // Device was inserted since last call to Input_Poll
    CONN_STATUS_INSERTED,

    // Constant: CONN_STATUS_ACTIVE
    // Must be requested by the application while in CONN_STATUS_INSERTED or CONN_STATUS_CONNECTED states.
    CONN_STATUS_ACTIVE,
  } CONN_STATUS;

  // Function: Input_Poll
  // Updates the device's connection status and InputState, if connected.
  virtual void poll() { mConnectionStatus = CONN_STATUS_UNKNOWN; }
  virtual void RumbleClear();
  virtual void RumbleTrigger(int amount);

  void setRumbleEnabled(bool enable) { mRumbleEnabled = enable; }
  // Each device can have it's own on and off and there there is a global setting
  void SetMasterRumbleEnabled(bool enable) { mMasterRumbleEnabled = enable; }

  CONN_STATUS GetConnectionStatus() const { return mConnectionStatus; }

  void Activate();
  void Deactivate();

  bool IsDisconnected() const;
  bool IsConnected() const;
  bool IsActive() const;

  const InputState& RefInputState() const { return mInputState; }
  InputState& RefInputState() { return mInputState; }
  int GetId() const { return mId; }
  void SetId(int id) { mId = id; }

protected:
  void Connect();
  void Disconnect();

  CONN_STATUS mConnectionStatus;

  InputState mInputState;
  InputMap mInputMap;
  bool mRumbleEnabled;
  bool mMasterRumbleEnabled;

  InputDevice() : mConnectionStatus(CONN_STATUS_UNKNOWN), mRumbleEnabled(true), mMasterRumbleEnabled(true) {}

  void SetInputMap(EMappedTriggerNames inch, ERawTriggerNames outch);

private:
  int mId;
};

struct InputChannel {
  InputChannel() { _value.Set<float>(0.0f); }
  svar64_t _value;
};

struct InputGroup {
  typedef std::map<std::string, InputChannel> channelmap_t;
  LockedResource<channelmap_t> _channels;

  struct setter {
    std::string _channelname;
    InputGroup* _group = nullptr;
    template <typename T> void as(const T& value) {
      assert(_group);
      _group->setAs<T>(_channelname, value);
    }
  };
  struct getter {
    ///////////////////////////////////
    template <typename T> attempt_cast<T> tryAs() {
      assert(_group);
      return _group->tryAs<T>(_channelname);
    }
    ///////////////////////////////////
    svar64_t rawValue() const {
      svar64_t rval;
      _group->_channels.atomicOp([&](channelmap_t& chmap) {
        if (chmap.find(_channelname) == chmap.end()) {
          printf("uhoh, cannot find channelname<%s>\n", _channelname.c_str());
          assert(false);
        }
        rval = chmap[_channelname]._value;
      });
      return rval;
    }
    ///////////////////////////////////
    std::string _channelname;
    InputGroup* _group = nullptr;
  };

  setter setChannel(const std::string& channelname) {
    setter rval;
    rval._channelname = channelname;
    rval._group = this;
    return rval;
  }
  getter getChannel(const std::string& channelname) {
    getter rval;
    rval._channelname = channelname;
    rval._group = this;
    return rval;
  }

  template <typename T> void setAs(const std::string& chname, const T& value) {
    _channels.atomicOp([&](channelmap_t& chmap) { chmap[chname]._value.Set<T>(value); });
  }
  template <typename T> T get(const std::string& chname) {
    T rval;
    _channels.atomicOp([&](channelmap_t& chmap) { rval = chmap[chname]._value.Get<T>(); });
    return rval;
  }
  template <typename T> attempt_cast<T> tryAs(const std::string& chname) {
    attempt_cast<T> rval(nullptr);
    _channels.atomicOp([&](channelmap_t& chmap) {
      rval = chmap[chname]._value.TryAs<T>();
    });
    return rval;
  }
};

struct InputManager : public NoRttiSingleton<InputManager> {
  typedef std::unordered_map<std::string, InputGroup*> inputgrp_map_t;
  ;

  static void poll();
  static void clearAll();
  static void setRumble(bool);

  InputManager();

  static void discoverDevices();

  static InputDevice* getInputDevice(size_t id);
  static const InputDevice* getKeyboardDevice() { return GetRef().mvpKeyboardInputDevice; }

  static void addDevice(InputDevice* pref) {
    pref->SetId((int)GetRef().mvpInputDevices.size());
    GetRef().mvpInputDevices.push_back(pref);
  }

  static size_t getNumberInputDevices() { return GetRef().mvpInputDevices.size(); }

  static InputGroup* inputGroup(const std::string& name);

private:
  InputDevice* mvpDipSwitchDevice;
  InputDevice* mvpKeyboardInputDevice;
  std::vector<InputDevice*> mvpInputDevices;
  LockedResource<inputgrp_map_t> _inputGroups;
};

}} // namespace ork::lev2
