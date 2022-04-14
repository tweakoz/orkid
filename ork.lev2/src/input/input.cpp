////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/input/inputdevice.h>

#if defined(ORK_CONFIG_IX)
#include "InputDeviceIX.h"
#elif defined(ORK_OSX)
#include "InputDeviceKeyboard.h"
#include "InputDeviceOSX.h"
#endif

namespace ork::lev2 {

void InputManager::addDevice(inputdevice_ptr_t pref) {
  _devices.atomicOp([pref](devvect_t& devs) {
    pref->SetId((int)devs.size());
    devs.push_back(pref);
  });
}

inputdevice_constptr_t InputManager::getKeyboardDevice() {
  return mvpKeyboardInputDevice;
}

size_t InputManager::getNumberInputDevices() {
  size_t rval = 0;
  _devices.atomicOp([&rval](devvect_t& devs) { rval = devs.size(); });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

RawInputKey InputMap::MapInput(const MappedInputKey& inkey) const {
  orkmap<MappedInputKey, RawInputKey>::const_iterator it = mInputMap.find(inkey);
  return (it == mInputMap.end()) ? RawInputKey(inkey.mKey) : it->second;
}

///////////////////////////////////////////////////////////////////////////////

void InputMap::set(ork::lev2::MappedInputKey inch, ork::lev2::RawInputKey outch) {
  mInputMap[inch] = outch;
}

///////////////////////////////////////////////////////////////////////////////

InputMap InputState::gInputMap;

///////////////////////////////////////////////////////////////////////////////

InputState::InputState() {
  for (int ival = 0; ival < KMAX_TRIGGERS; ival++) {
    _prevPressureValues[ival] = 0;
    _pressureValues[ival]     = 0;
    _pressureThresh[ival]     = 16;
    _triggerDown[ival]        = false;
    _triggerUp[ival]          = false;
    _triggerState[ival]       = false;
  }
}

///////////////////////////////////////////////////////////////////////////////

bool InputState::IsDown(MappedInputKey mapped, const InputMap& InputMap) const {
  // RawInputKey raw = InputMap.MapInput( mapped );
  int index = int(mapped.mKey);
  return _triggerState[index];
}

///////////////////////////////////////////////////////////////////////////////

bool InputState::IsUpEdge(MappedInputKey mapped, const InputMap& InputMap) const {
  RawInputKey raw = InputMap.MapInput(mapped);
  int index       = int(mapped.mKey);
  return _triggerUp[index];
}

///////////////////////////////////////////////////////////////////////////////

bool InputState::IsDownEdge(MappedInputKey mapped, const InputMap& InputMap) const {
  RawInputKey raw = InputMap.MapInput(mapped);
  int index       = int(mapped.mKey);
  OrkAssert(index < KMAX_TRIGGERS);
  return _triggerDown[index];
}

///////////////////////////////////////////////////////////////////////////////

F32 InputState::GetPressure(MappedInputKey mapped, const InputMap& InputMap) const {
  RawInputKey raw  = InputMap.MapInput(mapped);
  const F32 frecip = 1.0f / 127.0f;
  int index        = int(raw.mKey);
  OrkAssert(index < KMAX_TRIGGERS);
  float uval = _pressureValues[index];
  F32 fval   = frecip * (F32)uval;
  return F32(fval);
}

///////////////////////////////////////////////////////////////////////////////

void InputState::SetPressure(RawInputKey ch, float uVal) {

  int index = int(ch.mKey);
  OrkAssert(index < KMAX_TRIGGERS);
  float Thresh           = _pressureThresh[index];
  bool newstate          = (uVal > Thresh);
  _pressureValues[index] = uVal;
  _triggerState[index]   = newstate;
}

///////////////////////////////////////////////////////////////////////////////

float InputState::GetPressureRaw(int ch) const {
  return (_pressureValues[ch]);
}

///////////////////////////////////////////////////////////////////////////////

void InputState::SetPressureRaw(int ch, float uVal) {
  _pressureValues[ch] = uVal;
}

///////////////////////////////////////////////////////////////////////////////

void InputState::BeginCycle() {
}

///////////////////////////////////////////////////////////////////////////////

void InputState::Clear(int index) {
  OrkAssert(index < KMAX_TRIGGERS);

  if (index >= 0)
    _triggerState[index] = 0;
  else {
    for (int index = 0; index < KMAX_TRIGGERS; index++) {
      _triggerState[index] = 0;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void InputState::EndCycle() {
  for (int index = 0; index < KMAX_TRIGGERS; index++) {
    bool newstate = _triggerState[index];

    float Thresh = _pressureThresh[index];
    float OldVal = _prevPressureValues[index];

    bool oldstate = (OldVal > Thresh);
    if ((false == oldstate) && (true == newstate)) // key on
    {
      _triggerDown[index] = true;
      _triggerUp[index]   = false;
      // orkprintf( "KEYON<%d> %x\n", index,(void *) this );
    } else if ((true == oldstate) && (false == newstate)) // key off
    {
      _triggerDown[index] = false;
      _triggerUp[index]   = true;
      // orkprintf( "KEYOFF<%d> %x\n", index,(void *) this );
    } else {
      _triggerDown[index] = false;
      _triggerUp[index]   = false;
    }

    _prevPressureValues[index] = _pressureValues[index];
  }
}

///////////////////////////////////////////////////////////////////////////////

InputManager::InputManager() {
  discoverDevices();
}

///////////////////////////////////////////////////////////////////////////////

void InputManager::poll() {
  _devices.atomicOp([](devvect_t& devs) {
    for (auto device : devs)
      device->poll();
  });
}

///////////////////////////////////////////////////////////////////////////////

void InputManager::clearAll() {
  _devices.atomicOp([](devvect_t& devs) {
    for (auto device : devs)
      device->RefInputState().Clear(-1);
  });
}

///////////////////////////////////////////////////////////////////////////////

void InputManager::setRumble(bool mode) {
  _devices.atomicOp([mode](devvect_t& devs) {
    for (auto device : devs)
      device->SetMasterRumbleEnabled(mode);
  });
}

///////////////////////////////////////////////////////////////////////////////

inputdevice_ptr_t InputManager::getInputDevice(size_t id) {
  inputdevice_ptr_t rval;
  _devices.atomicOp([&rval, id](devvect_t& devs) {
    size_t numdevs = devs.size();
    if (id < numdevs) {
      rval = devs[id];
    }
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void InputManager::discoverDevices() {
#if defined(ORK_CONFIG_IX)
  auto pref = std::make_shared<InputDeviceIX>();
  addDevice(pref);
  mvpKeyboardInputDevice = pref;
#endif
}

///////////////////////////////////////////////////////////////////////////////

inputgroup_ptr_t InputManager::inputGroup(const std::string& name) {
  inputgroup_ptr_t rval = nullptr;
  _inputGroups.atomicOp([&](inputgrp_map_t& igmap) {
    auto it = igmap.find(name);
    if (it == igmap.end()) {
      rval        = std::make_shared<InputGroup>();
      igmap[name] = rval;
    } else {
      rval = it->second;
    }
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

inputmanager_ptr_t InputManager::instance() {
  struct concrete_manager : public InputManager {
    concrete_manager()
        : InputManager() {
    }
  };
  static inputmanager_ptr_t _ginst = std::make_shared<concrete_manager>();
  return _ginst;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void InputDevice::Activate() {
  if (mConnectionStatus == CONN_STATUS_CONNECTED || mConnectionStatus == CONN_STATUS_INSERTED)
    mConnectionStatus = CONN_STATUS_ACTIVE;
}

///////////////////////////////////////////////////////////////////////////////

void InputDevice::Deactivate() {
  if (mConnectionStatus == CONN_STATUS_ACTIVE)
    mConnectionStatus = CONN_STATUS_CONNECTED;
}

///////////////////////////////////////////////////////////////////////////////

bool InputDevice::IsDisconnected() const {
  return !IsConnected();
}

///////////////////////////////////////////////////////////////////////////////

bool InputDevice::IsConnected() const {
  return mConnectionStatus == CONN_STATUS_ACTIVE || mConnectionStatus == CONN_STATUS_CONNECTED ||
         mConnectionStatus == CONN_STATUS_INSERTED;
}

///////////////////////////////////////////////////////////////////////////////

bool InputDevice::IsActive() const {
  return mConnectionStatus == CONN_STATUS_ACTIVE;
}

///////////////////////////////////////////////////////////////////////////////

void InputDevice::Connect() {
  if (IsDisconnected())
    mConnectionStatus = CONN_STATUS_INSERTED;
  else if (mConnectionStatus == CONN_STATUS_INSERTED)
    mConnectionStatus = CONN_STATUS_CONNECTED;
}

///////////////////////////////////////////////////////////////////////////////

void InputDevice::Disconnect() {
  if (IsConnected())
    mConnectionStatus = CONN_STATUS_REMOVED;
  else if (mConnectionStatus == CONN_STATUS_REMOVED)
    mConnectionStatus = CONN_STATUS_DISCONNECTED;
}

///////////////////////////////////////////////////////////////////////////////

void InputDevice::SetInputMap(EMappedTriggerNames inch, ERawTriggerNames outch) {
  InputState::RefGlobalInputMap().set(inch, outch);
}

///////////////////////////////////////////////////////////////////////////////

void InputDevice::RumbleClear() {
}

///////////////////////////////////////////////////////////////////////////////

void InputDevice::RumbleTrigger(int amounmt) {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
