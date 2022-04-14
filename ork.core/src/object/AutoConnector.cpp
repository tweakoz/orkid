////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
    const char* SignalName, //
    autoconnector_ptr_t receiver,
    const char* SlotName) {

  OrkAssert(sender != nullptr);
  OrkAssert(receiver != nullptr);
  OrkAssert(sender != receiver);

  ork::PoolString psigname = ork::AddPooledString(SignalName);
  ork::PoolString psltname = ork::AddPooledString(SlotName);

  bool bOK = ork::object::Connect(sender, psigname, receiver, psltname);

  OrkAssert(bOK);

  if (bOK) {
    Connection* conn = new Connection;
    conn->_sender    = sender;
    conn->_reciever  = receiver;
    conn->mSignal    = psigname;
    conn->mSlot      = psltname;
    sender->_connections.insert(conn);
    receiver->_connections.insert(conn);
  }
}

void AutoConnector::setupSignalsAndSlots(autoconnector_ptr_t on) {
  object::ObjectClass* pclass                            = rtti::downcast<object::ObjectClass*>(on->GetClass());
  const reflect::Description& descript                   = pclass->Description();
  const reflect::Description::SignalMapType& signals     = descript.GetSignals();
  const reflect::Description::AutoSlotMapType& autoslots = descript.GetAutoSlots();
  const reflect::Description::FunctorMapType& functors   = descript.GetFunctors();

  for (reflect::Description::AutoSlotMapType::const_iterator it = autoslots.begin(); it != autoslots.end(); it++) {
    const ork::ConstString& slotname                     = it->first;
    ork::object::AutoSlot ork::Object::*const ptr2slotmp = it->second;
    ork::object::AutoSlot& slot                          = on->*ptr2slotmp;
    slot.SetSlotName(ork::AddPooledString(slotname.c_str()));
    slot.SetObject(on);
  }
  // for( reflect::Description::AutoSlotMapType::const_iterator it=autoslots.begin(); it!=autoslots.end(); it++ )
  //{	const ork::PoolString& slotname = it->first;
  //	AutoSlot* ptr2slot = it->second;
  //	ptr2slot->SetName( slotname );
  //	ptr2slot->SetObject( this );
  //}
}

} // namespace ork
