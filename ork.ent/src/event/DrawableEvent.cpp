////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/event/DrawableEvent.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::DrawableEvent, "DrawableEvent");

namespace ork { namespace ent { namespace event {
	
void DrawableEvent::Describe()
{
	reflect::RegisterProperty("Event", &DrawableEvent::mEvent);
}

} } } // ork::ent::event
