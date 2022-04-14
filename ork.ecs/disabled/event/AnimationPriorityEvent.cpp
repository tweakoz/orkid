////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/event/AnimationPriorityEvent.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>

namespace ork { namespace ent { namespace event {

AnimationPriority::AnimationPriority(ork::PieceString name, int priority)
    : mName(ork::AddPooledString(name))
    , mPriority(priority) {
}

}}} // namespace ork::ent::event
