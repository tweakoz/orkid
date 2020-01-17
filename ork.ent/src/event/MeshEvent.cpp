////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/event/MeshEvent.h>

#include <ork/reflect/RegisterProperty.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::MeshEnableEvent, "MeshEnableEvent");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::MeshLayerFxEvent, "MeshLayerFxEvent");

namespace ork { namespace ent { namespace event {
	
void MeshEnableEvent::Describe()
{
	reflect::RegisterProperty("Name", &MeshEnableEvent::mName);
	reflect::RegisterProperty("Enable", &MeshEnableEvent::mEnable);
}

void MeshLayerFxEvent::Describe()
{
	reflect::RegisterProperty("Name", &MeshLayerFxEvent::mName);
	reflect::RegisterProperty("Enable", &MeshLayerFxEvent::mEnable);
}

} } } // ork::ent::event
