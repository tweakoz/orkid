////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/object/Object.h>
#include <ork/object/connect.h>

namespace ork {

class AutoConnector;

struct Connection
{
	AutoConnector*	mpSender;
	AutoConnector*	mpReciever;
	PoolString		mSignal;
	PoolString		mSlot;

	Connection() : mpSender(0), mpReciever(0), mSignal(), mSlot() {}
};

class AutoConnector : public Object
{
	RttiDeclareAbstract(AutoConnector,Object);
public:
	AutoConnector();
	~AutoConnector();
	void Connect( const char* SignalName, AutoConnector* pReciever, const char* SlotName );
	void DisconnectAll();

	void SetupSignalsAndSlots();

private:
	orkset<Connection*>	mConnections;
};

}
