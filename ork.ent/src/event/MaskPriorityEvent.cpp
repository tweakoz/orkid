////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/event/MaskPriorityEvent.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::MaskPriority, "MaskPriority");

namespace ork { namespace ent { namespace event {

void MaskPriority::Describe()
{
	ork::reflect::RegisterProperty("MaskName", &MaskPriority::mMaskName);
	ork::reflect::RegisterProperty("Priority", &MaskPriority::mPriority);
}

MaskPriority::MaskPriority(ork::PieceString maskname, int priority) : mMaskName(ork::AddPooledString(maskname)), mPriority(priority)
{
}

} } } // namespace ork::ent::event
