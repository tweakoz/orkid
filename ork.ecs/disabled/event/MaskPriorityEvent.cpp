////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/event/MaskPriorityEvent.h>
#include <ork/application/application.h>

namespace ork { namespace ent { namespace event {

MaskPriority::MaskPriority(ork::PieceString maskname, int priority)
    : mMaskName(ork::AddPooledString(maskname))
    , mPriority(priority) {
}

}}} // namespace ork::ent::event
