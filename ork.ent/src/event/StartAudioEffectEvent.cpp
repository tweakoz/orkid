////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/AudioComponent.h>
#include <pkg/ent/event/StartAudioEffectEvent.h>
#include <ork/application/application.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent { namespace event {
///////////////////////////////////////////////////////////////////////////////
PlaySoundEvent::PlaySoundEvent(ork::PieceString name)
    : mSoundName(ork::AddPooledString(name)) {
}
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::ent::event
///////////////////////////////////////////////////////////////////////////////
