////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_ENT_EVENT_MESHENABLEEVENT_H
#define ORK_ENT_EVENT_MESHENABLEEVENT_H

#include <ork/rtti/RTTI.h>
#include <ork/event/Event.h>
#include <ork/application/application.h>

namespace ork { namespace ent { namespace event {

class MeshEnableEvent : public ork::event::Event
{
	RttiDeclareConcrete(MeshEnableEvent, ork::event::Event);
public:

	MeshEnableEvent(ork::PieceString name = "", bool enable = true) : mName(ork::AddPooledString(name)), mEnable(enable) {}

	void SetName(ork::PoolString name) { mName = name; }
	ork::PoolString GetName() const { return mName; }

	void SetEnable(bool enable) { mEnable = enable; }
	bool IsEnable() const { return mEnable; }

private:
	ork::PoolString mName;
	bool mEnable;
};

class MeshLayerFxEvent : public ork::event::Event
{
	RttiDeclareConcrete(MeshLayerFxEvent, ork::event::Event);
public:

	MeshLayerFxEvent(ork::PieceString name = "", bool enable = true) : mName(ork::AddPooledString(name)), mEnable(enable) {}

	void SetName(ork::PoolString name) { mName = name; }
	ork::PoolString GetName() const { return mName; }

	void SetEnable(bool enable) { mEnable = enable; }
	bool IsEnable() const { return mEnable; }

private:
	ork::PoolString mName;
	bool mEnable;
};

} } } // ork::ent::event

#endif // ORK_ENT_EVENT_MESHENABLEEVENT_H
