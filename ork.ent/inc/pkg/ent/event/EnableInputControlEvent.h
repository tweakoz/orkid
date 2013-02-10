////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_ENT_EVENT_ENABLEINPUTCONTROLLABLE_H
#define ORK_ENT_EVENT_ENABLEINPUTCONTROLLABLE_H

#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>

#include <ork/event/Event.h>

namespace ork { namespace ent { namespace event {

class EnableInputControlEvent : public ork::event::Event
{
	RttiDeclareAbstract(EnableInputControlEvent,ork::event::Event);

public:

	EnableInputControlEvent(bool enable = true);

	void SetEnable(bool enable);
	bool IsEnable() const;

private:

	bool mEnable;

	virtual Object *Clone() const
	{
		return new EnableInputControlEvent(mEnable);
	}
};

} } } // ork::ent::event

#endif // ORK_ENT_EVENT_ENABLEINPUTCONTROLLABLE_H
