////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/event/EnableInputControlEvent.h>

#include <ork/reflect/RegisterProperty.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::event::EnableInputControlEvent, "EnableInputControlEvent");

namespace ork { namespace ent { namespace event {

void EnableInputControlEvent::Describe()
{
	ork::reflect::RegisterProperty("Enable", &EnableInputControlEvent::mEnable);
}

EnableInputControlEvent::EnableInputControlEvent(bool enable) : mEnable(enable)
{
}

void EnableInputControlEvent::SetEnable(bool enable)
{
	mEnable = enable;
}

bool EnableInputControlEvent::IsEnable() const
{
	return mEnable;
}

Object* EnableInputControlEvent::Clone() const // final
{
    return new EnableInputControlEvent(mEnable);
}

} } } // namespace prodigy::ent::event
