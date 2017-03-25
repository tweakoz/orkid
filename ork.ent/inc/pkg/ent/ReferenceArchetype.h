////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <ork/rtti/RTTI.h>

#include <ork/asset/Asset.h>
#include <ork/asset/AssetManager.h>

#include <pkg/ent/entity.h>

namespace ork { namespace ent {

class ArchetypeAsset : public asset::Asset
{
	RttiDeclareConcrete( ArchetypeAsset, asset::Asset );
public:

	ArchetypeAsset();
	~ArchetypeAsset() final;

	Archetype* GetArchetype() const { return mArchetype; }
	void SetArchetype(Archetype* archetype) { mArchetype = archetype; }

protected:
	Archetype* mArchetype;
};

///////////////////////////////////////////////////////////

class ReferenceArchetype : public Archetype
{
	RttiDeclareConcrete( ReferenceArchetype, Archetype );

public:

	ReferenceArchetype();

	ArchetypeAsset* GetAsset() const { return mArchetypeAsset; }
	void SetAsset(ArchetypeAsset* passet) { mArchetypeAsset = passet; }

private:
	void DoCompose(ork::ent::ArchComposer& composer) final;
	void DoComposeEntity( Entity *pent ) const final ;
	void DoLinkEntity(SceneInst* inst, Entity *pent) const final;
	void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const final;
	void DoStopEntity(SceneInst* psi, Entity *pent) const final;

	void DoComposePooledEntities(SceneInst *inst);
	void DoLinkPooledEntities(SceneInst *inst);

	ArchetypeAsset* mArchetypeAsset;
};

} }
