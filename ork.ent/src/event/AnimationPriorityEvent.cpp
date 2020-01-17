////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/event/AnimationPriorityEvent.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::AnimationPriority, "AnimationPriority");

namespace ork { namespace ent { namespace event {

void AnimationPriority::Describe()
{
	ork::reflect::RegisterProperty("Name", &AnimationPriority::mName);
	ork::reflect::RegisterProperty("Priority", &AnimationPriority::mPriority);
}

AnimationPriority::AnimationPriority(ork::PieceString name, int priority) : mName(ork::AddPooledString(name)), mPriority(priority)
{
}

Object* AnimationPriority::Clone() const // final
{
    return new AnimationPriority(mName,mPriority);
}

} } } // namespace ork::ent::event
