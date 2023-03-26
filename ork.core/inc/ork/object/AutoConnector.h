////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/object/connect.h>

namespace ork {

class AutoConnector;
using autoconnector_ptr_t = AutoConnector*;

struct Connection {
  autoconnector_ptr_t _sender;
  autoconnector_ptr_t _reciever;
  PoolString mSignal;
  PoolString mSlot;

  Connection()
      : _sender(nullptr)
      , _reciever(nullptr)
      , mSignal()
      , mSlot() {
  }
};

class AutoConnector : public Object {
  RttiDeclareAbstract(AutoConnector, Object);

public:
  AutoConnector();
  ~AutoConnector();
  static void connect(
      autoconnector_ptr_t sender,
      const char* SignalName, //
      autoconnector_ptr_t receiver,
      const char* SlotName);

  static void disconnectAll(autoconnector_ptr_t on);

  static void setupSignalsAndSlots(autoconnector_ptr_t on);

private:
  orkset<Connection*> _connections;
};

} // namespace ork
