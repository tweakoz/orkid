///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/config/config.h>
#include <ork/object/Object.h>
#include <ork/kernel/orklut.h>
#include <ork/kernel/core/singleton.h>

namespace ork { namespace event {

class Event;

class Channel
{
public:

	void AddListener( Object* plistener );
	void RemoveListener( Object* plistener );
	void BroadcastNotify( const Event* pEV );
	void BroadcastQuery( Event* pEV );

	Channel();

private:

	ork::recursive_mutex mChannelLock;
	std::set<Object*> mListeners;
};

class Broadcaster : public NoRttiSingleton<Broadcaster> 
{
public:

	void AddListenerOnChannel( Object* plistener, const PoolString& channame );
	void RemoveListenerOnChannel( Object* plistener, const PoolString& channame );
	void BroadcastNotifyOnChannel( const Event* pEV, const PoolString& channame );
	void BroadcastQueryOnChannel( Event* pEV, const PoolString& channame );

	Broadcaster();

private:

	Channel* GetChannel( const PoolString& named );

	ork::recursive_mutex mChannelsLock;
	orklut<PoolString,Channel*> mChannels;
};

} } // ork::event
