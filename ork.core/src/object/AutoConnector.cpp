////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/object/AutoConnector.h>
#include <ork/rtti/Class.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/rtti/downcast.h>

#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/reflect/serialize/BinarySerializer.h>
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
void AutoConnector::DisconnectAll() {
  int inumcon = mConnections.size();

  while (false == mConnections.empty()) {
    Connection* conn = *mConnections.begin();

    bool bOK = ork::object::Disconnect(conn->mpSender, conn->mSignal, conn->mpReciever, conn->mSlot);
    OrkAssert(bOK);

    ////////////////////////////////////////////////////
    // remove from my connection list
    ////////////////////////////////////////////////////
    mConnections.erase(mConnections.begin());
    ////////////////////////////////////////////////////

    ////////////////////////////////////////////////////
    // remove from recievers connection list
    ////////////////////////////////////////////////////
    orkset<Connection*>::iterator itoth;
    if (this == conn->mpSender) {
      itoth = conn->mpReciever->mConnections.find(conn);
      if (itoth != conn->mpReciever->mConnections.end()) {
        mConnections.erase(itoth); // remove from other connection list
      }
    } else if (this == conn->mpReciever) {
      itoth = conn->mpSender->mConnections.find(conn);
      if (itoth != conn->mpSender->mConnections.end()) {
        mConnections.erase(itoth); // remove from other connection list
      }
    }
    ////////////////////////////////////////////////////

    delete conn;
  }
}

void AutoConnector::Connect(const char* SignalName, AutoConnector* pReciever, const char* SlotName) {
  ork::PoolString psigname = ork::AddPooledString(SignalName);
  ork::PoolString psltname = ork::AddPooledString(SlotName);

  bool bOK = ork::object::Connect(this, psigname, pReciever, psltname);

  OrkAssert(bOK);

  if (bOK) {
    Connection* conn = new Connection;
    conn->mpSender   = this;
    conn->mpReciever = pReciever;
    conn->mSignal    = psigname;
    conn->mSlot      = psltname;
    mConnections.insert(conn);
    if (pReciever != this) {
      pReciever->mConnections.insert(conn);
    }
  }
}

void AutoConnector::SetupSignalsAndSlots() {
  object::ObjectClass* pclass                            = rtti::downcast<object::ObjectClass*>(GetClass());
  const reflect::Description& descript                   = pclass->Description();
  const reflect::Description::SignalMapType& signals     = descript.GetSignals();
  const reflect::Description::AutoSlotMapType& autoslots = descript.GetAutoSlots();
  const reflect::Description::FunctorMapType& functors   = descript.GetFunctors();

  for (reflect::Description::AutoSlotMapType::const_iterator it = autoslots.begin(); it != autoslots.end(); it++) {
    const ork::ConstString& slotname                     = it->first;
    ork::object::AutoSlot ork::Object::*const ptr2slotmp = it->second;
    ork::object::AutoSlot& slot                          = this->*ptr2slotmp;
    slot.SetSlotName(ork::AddPooledString(slotname.c_str()));
    slot.SetObject(this);
  }
  // for( reflect::Description::AutoSlotMapType::const_iterator it=autoslots.begin(); it!=autoslots.end(); it++ )
  //{	const ork::PoolString& slotname = it->first;
  //	AutoSlot* ptr2slot = it->second;
  //	ptr2slot->SetName( slotname );
  //	ptr2slot->SetObject( this );
  //}
}

} // namespace ork
