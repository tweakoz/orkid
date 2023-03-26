////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/AutoConnector.h>
#include <ork/rtti/Class.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.inl>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/rtti/downcast.h>

#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/kernel/string/string.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::AutoConnector, "AutoConnector");

namespace ork {
///////////////////////////////////////////////////////////////////////////////

void AutoConnector::Describe() {
}

AutoConnector::AutoConnector() {
}
AutoConnector::~AutoConnector() {
}
void AutoConnector::disconnectAll(autoconnector_ptr_t on) {
  int inumcon = on->_connections.size();

  while (false == on->_connections.empty()) {
    Connection* conn = *on->_connections.begin();

    bool bOK = ork::object::Disconnect(conn->_sender, conn->mSignal, conn->_reciever, conn->mSlot);
    OrkAssert(bOK);

    ////////////////////////////////////////////////////
    // remove from my connection list
    ////////////////////////////////////////////////////
    on->_connections.erase(on->_connections.begin());
    ////////////////////////////////////////////////////

    ////////////////////////////////////////////////////
    // remove from recievers connection list
    ////////////////////////////////////////////////////
    orkset<Connection*>::iterator itoth;
    if (on == conn->_sender) {
      itoth = conn->_reciever->_connections.find(conn);
      if (itoth != conn->_reciever->_connections.end()) {
        on->_connections.erase(itoth); // remove from other connection list
      }
    } else if (on == conn->_reciever) {
      itoth = conn->_sender->_connections.find(conn);
      if (itoth != conn->_sender->_connections.end()) {
        on->_connections.erase(itoth); // remove from other connection list
      }
    }
    ////////////////////////////////////////////////////

    delete conn;
  }
}

void AutoConnector::connect( // static
    autoconnector_ptr_t sender,
    std::string SignalName, //
    autoconnector_ptr_t receiver,
    std::string SlotName) {

  OrkAssert(sender != nullptr);
  OrkAssert(receiver != nullptr);
  OrkAssert(sender != receiver);

  bool bOK = ork::object::Connect(sender, SignalName, receiver, SlotName);

  OrkAssert(bOK);

  if (bOK) {
    Connection* conn = new Connection;
    conn->_sender    = sender;
    conn->_reciever  = receiver;
    conn->mSignal    = SignalName;
    conn->mSlot      = SlotName;
    sender->_connections.insert(conn);
    receiver->_connections.insert(conn);
  }
}

void AutoConnector::setupSignalsAndSlots(autoconnector_ptr_t on) {
  using namespace object;
  using namespace reflect;
  ObjectClass* pclass                            = rtti::downcast<ObjectClass*>(on->GetClass());
  const Description& descript                   = pclass->Description();
  const Description::SignalMapType& signals     = descript.GetSignals();
  const Description::AutoSlotMapType& autoslots = descript.GetAutoSlots();
  const Description::FunctorMapType& functors   = descript.GetFunctors();

  for (auto it : autoslots ) {
    const auto& slotname                     = it.first;
    AutoSlot Object::*const ptr2slotmp = it.second;
    auto& slot                          = on.get()->*ptr2slotmp;
    slot.SetSlotName(slotname);
    slot.attach(on);
  }
  // for( Description::AutoSlotMapType::const_iterator it=autoslots.begin(); it!=autoslots.end(); it++ )
  //{	const ork::PoolString& slotname = it->first;
  //	AutoSlot* ptr2slot = it->second;
  //	ptr2slot->SetName( slotname );
  //	ptr2slot->SetObject( this );
  //}
}

} // namespace ork
