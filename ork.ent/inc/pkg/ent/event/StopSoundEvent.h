////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/event/Event.h>

namespace ork { namespace ent { namespace event {

class StopSoundEvent : public ork::event::Event {
public:
  StopSoundEvent(ork::PieceString name = "");

  void SetName(ork::PieceString name);
  ork::PoolString GetName() const;

  const ork::PoolString& GetSoundName() const {
    return mSoundName;
  }

private:
  ork::PoolString mSoundName;
};

}}} // namespace ork::ent::event
