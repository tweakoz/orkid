////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/event/Event.h>

namespace ork { namespace ent { namespace event {

class PlayAnimationEvent : public ork::event::Event {

public:
  PlayAnimationEvent(
      ork::PieceString maskname = "",
      ork::PieceString name     = "",
      int priority              = 0,
      float speed               = 1.0f,
      float interp_duration     = 0.0f,
      bool loop                 = true);

  void SetMaskName(ork::PieceString name);
  ork::PoolString GetMaskName() const;

  void SetName(ork::PieceString name);
  ork::PoolString GetName() const;

  void SetPriority(int priority);
  int GetPriority() const;

  void SetSpeed(float speed);
  float GetSpeed() const;

  void SetInterpDuration(float interp_duration);
  float GetInterpDuration() const;

  void SetLoop(bool loop);
  bool IsLoop() const;

private:
  ork::PoolString mMaskName;
  ork::PoolString mName;
  int mPriority;
  float mSpeed;
  float mInterpDuration;
  bool mLoop;
};

}}} // namespace ork::ent::event
