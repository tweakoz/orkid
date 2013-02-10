////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_ENT_EVENT_STARTAUDIOEFFECT_H
#define ORK_ENT_EVENT_STARTAUDIOEFFECT_H

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>

#include <ork/event/Event.h>

namespace ork { namespace ent { namespace event {

class PlaySoundEvent : public ork::event::Event
{
	RttiDeclareAbstract(PlaySoundEvent,ork::event::Event);
public:

	PlaySoundEvent(ork::PieceString name = "");

	void SetName(ork::PoolString name) {mSoundName = name;}
	ork::PoolString GetName() const;

	const ork::PoolString& GetSoundName() const { return mSoundName; }

private:

	ork::PoolString		mSoundName;

	virtual Object *Clone() const
	{
		return new PlaySoundEvent(mSoundName);
	}
};

/*class PlaySoundEventEx : public ork::rtti::RTTI<PlaySoundEvent, ork::event::Event>
{
public:
	static void Describe();

	PlaySoundEvent(ork::PieceString name = "");

	void SetName(ork::PieceString name);
	ork::PoolString GetName() const;

	int GetNote() const { return miNote; }
	int GetVelocity() const { return miVelocity; }
	const ork::PoolString& GetSoundName() const { return mSoundName; }

private:

	ork::PoolString		mSoundName;
	int					miNote;
	int					miVelocity;
};*/

} } } // ork::ent::event

#endif // ORK_ENT_EVENT_PLAYANIMATION_H
