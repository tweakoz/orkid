////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/event/Event.h>

namespace ork { namespace ent { namespace event {

class DrawableEvent : public ork::event::Event {
public:
  DrawableEvent(Event* event = NULL)
      : mEvent(event) {
  }

  void SetEvent(Event* event) {
    mEvent = event;
  }
  Event* GetEvent() const {
    return mEvent;
  }

private:
  ork::event::Event* mEvent;
};

}}} // namespace ork::ent::event
