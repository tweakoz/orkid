////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_ENT_EVENT_CHANGEANIMATIONSPEED_H
#define ORK_ENT_EVENT_CHANGEANIMATIONSPEED_H

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>

#include <ork/event/Event.h>

namespace ork { namespace ent { namespace event {

class ChangeAnimationSpeedEvent : public ork::event::Event
{
	RttiDeclareAbstract(ChangeAnimationSpeedEvent,ork::event::Event);

public:

	ChangeAnimationSpeedEvent(float speed = 1.0f);

	void SetSpeed(float speed);
	float GetSpeed() const;

private:

	float mSpeed;
};

} } } // ork::ent::event

#endif // ORK_ENT_EVENT_CHANGEANIMATIONSPEED_H
