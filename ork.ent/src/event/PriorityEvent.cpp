////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/event/PriorityEvent.h>

#include <ork/reflect/RegisterProperty.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::PriorityEvent, "PriorityEvent");

namespace ork { namespace ent { namespace event {

void PriorityEvent::Describe()
{
	ork::reflect::RegisterProperty("Priority", &PriorityEvent::mPriority);
}

PriorityEvent::PriorityEvent(int priority) : mPriority(priority)
{
}

} } } // namespace ork::ent::event
