////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/event/AnimFinishEvent.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>

namespace ork { namespace ent { namespace event {

AnimFinishEvent::AnimFinishEvent(ork::PieceString name)
    : mName(ork::AddPooledString(name)) {
}

void AnimFinishEvent::SetName(ork::PieceString name) {
  mName = ork::AddPooledString(name);
}

ork::PoolString AnimFinishEvent::GetName() const {
  return mName;
}

}}} // namespace ork::ent::event
