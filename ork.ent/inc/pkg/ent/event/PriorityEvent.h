////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/event/Event.h>

namespace ork { namespace ent { namespace event {

class PriorityEvent : public ork::event::Event {

public:
  PriorityEvent(int priority = 0);

  inline void SetPriority(int priority) {
    mPriority = priority;
  }
  inline int GetPriority() const {
    return mPriority;
  }

private:
  int mPriority;
};

}}} // namespace ork::ent::event
