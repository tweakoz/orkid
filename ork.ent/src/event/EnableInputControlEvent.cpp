////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/event/EnableInputControlEvent.h>

namespace ork { namespace ent { namespace event {

EnableInputControlEvent::EnableInputControlEvent(bool enable)
    : mEnable(enable) {
}

void EnableInputControlEvent::SetEnable(bool enable) {
  mEnable = enable;
}

bool EnableInputControlEvent::IsEnable() const {
  return mEnable;
}

}}} // namespace ork::ent::event
