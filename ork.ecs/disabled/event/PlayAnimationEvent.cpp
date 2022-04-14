////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/event/PlayAnimationEvent.h>
#include <ork/application/application.h>

namespace ork { namespace ent { namespace event {

PlayAnimationEvent::PlayAnimationEvent(
    ork::PieceString maskname,
    ork::PieceString name,
    int priority,
    float speed,
    float interp_duration,
    bool loop)
    : mMaskName(ork::AddPooledString(maskname))
    , mName(ork::AddPooledString(name))
    , mPriority(priority)
    , mSpeed(speed)
    , mInterpDuration(interp_duration)
    , mLoop(loop) {
}

void PlayAnimationEvent::SetMaskName(ork::PieceString maskname) {
  mMaskName = ork::AddPooledString(maskname);
}

ork::PoolString PlayAnimationEvent::GetMaskName() const {
  return mMaskName;
}

void PlayAnimationEvent::SetName(ork::PieceString name) {
  mName = ork::AddPooledString(name);
}

ork::PoolString PlayAnimationEvent::GetName() const {
  return mName;
}

void PlayAnimationEvent::SetPriority(int priority) {
  mPriority = priority;
}

int PlayAnimationEvent::GetPriority() const {
  return mPriority;
}

void PlayAnimationEvent::SetSpeed(float speed) {
  mSpeed = speed;
}

float PlayAnimationEvent::GetSpeed() const {
  return mSpeed;
}

void PlayAnimationEvent::SetInterpDuration(float interp_duration) {
  mInterpDuration = interp_duration;
}

float PlayAnimationEvent::GetInterpDuration() const {
  return mInterpDuration;
}

void PlayAnimationEvent::SetLoop(bool loop) {
  mLoop = loop;
}

bool PlayAnimationEvent::IsLoop() const {
  return mLoop;
}

}}} // namespace ork::ent::event
