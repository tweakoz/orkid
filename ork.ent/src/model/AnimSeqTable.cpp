////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>
#include <pkg/ent/AnimSeqTable.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AnimSeqTable, "AnimSeqTable");

template class ork::reflect::DirectTypedMap<ork::orklut<float, ork::event::Event*> >;

namespace ork { namespace ent {

void AnimSeqTable::Describe()
{
	ork::reflect::RegisterMapProperty("EventTable", &AnimSeqTable::mEventTable);
	ork::reflect::annotatePropertyForEditor<AnimSeqTable>("EventTable", "editor.factorylistbase", "Event");
}

AnimSeqTable::AnimSeqTable() : mEventTable(ork::EKEYPOLICY_MULTILUT) {}

AnimSeqTable::~AnimSeqTable()
{
	for( ork::orklut<float, ork::event::Event*>::const_iterator
			it=mEventTable.begin();
			it!=mEventTable.end();
			it++ 
	)
	{
		ork::event::Event* pev = it->second;
		delete pev;
	}

}

void AnimSeqTable::AddEvent(float keyframe, ork::event::Event *event)
{
	mEventTable.AddSorted(keyframe, event);
}

void AnimSeqTable::CopyEvents(const AnimSeqTable* pase)
{
	for(ork::orklut<float, ork::event::Event*>::const_iterator it = pase->mEventTable.begin(); it != pase->mEventTable.end(); it++)
		mEventTable.AddSorted((*it).first, (*it).second);
}

void AnimSeqTable::NotifyListener(float previousKeyFrame, float currentKeyFrame, ork::Object* listener)
{
	if(previousKeyFrame == currentKeyFrame)
		return;
	else if(previousKeyFrame < currentKeyFrame)
	{
		for(ork::orklut<float, ork::event::Event*>::const_iterator it = mEventTable.UpperBound(previousKeyFrame);
				it != mEventTable.end() && (*it).first <= currentKeyFrame;
				it++)
			listener->Notify((*it).second);
	}
	else
	{
		for(ork::orklut<float, ork::event::Event*>::const_iterator it = mEventTable.LowerBound(previousKeyFrame);
				it != mEventTable.end() && (*it).first >= currentKeyFrame;
				it--)
			listener->Notify((*it).second);
	}
}

} }
