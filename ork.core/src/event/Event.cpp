////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/event/Event.h>
#include <ork/kernel/orklut.hpp>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::event::Event, "Event");
INSTANTIATE_TRANSPARENT_RTTI(ork::event::VEvent, "VEvent");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace event {
///////////////////////////////////////////////////////////////////////////////

void Event::Describe()
{

}
void VEvent::Describe()
{

}

///////////////////////////////////////////////////////////////////////////////
Channel::Channel()
	: mChannelLock( "ChannelLock" )
{
}
void Channel::AddListener( Object* plistener )
{
	mChannelLock.Lock();
	mListeners.insert(plistener);
	mChannelLock.UnLock();
}
void Channel::RemoveListener( Object* plistener )
{
	mChannelLock.Lock();
	std::set<Object*>::iterator it=mListeners.find( plistener );
	if( it!=mListeners.end() )
	{
		mListeners.erase(it);
	}
	mChannelLock.UnLock();
}
void Channel::BroadcastNotify( const Event* pEV )
{
	mChannelLock.Lock();
	for( std::set<Object*>::const_iterator it=mListeners.begin(); it!=mListeners.end(); it++ )
	{
		Object* pOBJ = *it;
		pOBJ->Notify( pEV );
	}
	mChannelLock.UnLock();
}
void Channel::BroadcastQuery( Event* pEV )
{
	mChannelLock.Lock();
	for( std::set<Object*>::const_iterator it=mListeners.begin(); it!=mListeners.end(); it++ )
	{
		Object* pOBJ = *it;
		pOBJ->Query( pEV );
	}
	mChannelLock.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
Broadcaster::Broadcaster()
	: mChannelsLock( "ChannelsLock" )
{
}
Channel* Broadcaster::GetChannel( const PoolString& named )
{
	Channel* pchan = nullptr;
	mChannelsLock.Lock();
	{
		orklut<PoolString,Channel*>::const_iterator it = mChannels.find(named);
		if( it==mChannels.end() )
		{	pchan = new Channel;
			mChannels.AddSorted( named, pchan );
		}
		else
			pchan = it->second;
	}
	mChannelsLock.UnLock();
	return pchan;
}

void Broadcaster::AddListenerOnChannel( Object* plistener, const PoolString& named )
{
	Channel* pchan = GetChannel( named );
	pchan->AddListener( plistener );
}
void Broadcaster::RemoveListenerOnChannel( Object* plistener, const PoolString& named )
{
	Channel* pchan = GetChannel( named );
	pchan->RemoveListener( plistener );
}
void Broadcaster::BroadcastNotifyOnChannel( const Event* pEV, const PoolString& named )
{
	Channel* pchan = GetChannel( named );
	pchan->BroadcastNotify( pEV );
}
void Broadcaster::BroadcastQueryOnChannel( Event* pEV, const PoolString& named )
{
	Channel* pchan = GetChannel( named );
	pchan->BroadcastQuery( pEV );
}

///////////////////////////////////////////////////////////////////////////////
} } // ork::event
///////////////////////////////////////////////////////////////////////////////
