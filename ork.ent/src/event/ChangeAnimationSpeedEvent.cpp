////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/event/ChangeAnimationSpeedEvent.h>

#include <ork/reflect/properties/register.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::ChangeAnimationSpeedEvent, "ChangeAnimationSpeedEvent");

namespace ork { namespace ent { namespace event {
	
void ChangeAnimationSpeedEvent::Describe()
{
	ork::reflect::RegisterProperty("Speed", &ChangeAnimationSpeedEvent::mSpeed);
}

ChangeAnimationSpeedEvent::ChangeAnimationSpeedEvent(float speed) : mSpeed(speed)
{
}

void ChangeAnimationSpeedEvent::SetSpeed(float speed)
{
	mSpeed = speed;
}

float ChangeAnimationSpeedEvent::GetSpeed() const
{
	return mSpeed;
}

} } } // namespace ork::ent::event
