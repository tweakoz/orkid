////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/entity.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {
///////////////////////////////////////////////////////////////////////////////

class WorldControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( WorldControllerData, ent::ComponentData );

	ent::ComponentInst* createComponent(ent::Entity* pent) const override;

public:

	WorldControllerData();
};

///////////////////////////////////////////////////////////////////////////////

class WorldControllerInst : public ent::ComponentInst
{
	const WorldControllerData&			mPcd;

	void DoUpdate(ent::SceneInst* sinst) override;

public:

	WorldControllerInst( const WorldControllerData& pcd, ent::Entity *entity );

};

///////////////////////////////////////////////////////////////////////////////

class WorldArchetype : public ork::ent::Archetype
{
	RttiDeclareConcrete( WorldArchetype, ent::Archetype );

	void DoLinkEntity( ent::SceneInst* psi, ent::Entity *pent ) const override;
	void DoCompose(ork::ent::ArchComposer& composer) override;
	void DoStartEntity( ork::ent::SceneInst* psi, const ork::fmtx4& mtx, ork::ent::Entity* pent ) const override;

public:

	WorldArchetype();


};

///////////////////////////////////////////////////////////////////////////////

} }