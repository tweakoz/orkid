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
using autoconnector_ptr_t = std::shared_ptr<AutoConnector>;

struct Connection {
  autoconnector_ptr_t _sender;
  autoconnector_ptr_t _reciever;
  std::string mSignal;
  std::string mSlot;

  Connection()
      : _sender(nullptr)
      , _reciever(nullptr){
  }
};

class AutoConnector : public Object {
  RttiDeclareAbstract(AutoConnector, Object);

public:
  AutoConnector();
  ~AutoConnector();
  static void connect(
      autoconnector_ptr_t sender,
      std::string SignalName, //
      autoconnector_ptr_t receiver,
      std::string SlotName);

  static void disconnectAll(autoconnector_ptr_t on);

  static void setupSignalsAndSlots(autoconnector_ptr_t on);

private:
  orkset<Connection*> _connections;
};

} // namespace ork
