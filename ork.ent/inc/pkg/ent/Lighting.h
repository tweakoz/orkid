////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/entity.h>
#include <ork/math/cvector3.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class LightingSystemData : public ork::ent::SystemData
{
	RttiDeclareConcrete(LightingSystemData, ork::ent::SystemData);

public:
	///////////////////////////////////////////////////////
	LightingSystemData();
	///////////////////////////////////////////////////////

	const ork::lev2::LightManagerData& Lmd() const { return mLmd; }

private:

    ork::ent::System* CreateComponentInst(ork::ent::SceneInst *pinst) const final;  
	ork::Object* LmdAccessor() { return & mLmd; }

	ork::lev2::LightManagerData	mLmd;

};

///////////////////////////////////////////////////////////////////////////////

class LightingManagerComponentInst : public ork::ent::System
{
	RttiDeclareAbstract(LightingManagerComponentInst, ork::ent::ComponentInst);

public:

	LightingManagerComponentInst( const LightingSystemData &data, ork::ent::SceneInst *pinst );

	ork::lev2::LightManager& GetLightManager() { return mLightManager; }

private:

	ork::lev2::LightManager		mLightManager;
};

///////////////////////////////////////////////////////////////////////////////

class LightingComponentData : public ork::ent::ComponentData
{
	RttiDeclareConcrete(LightingComponentData, ork::ent::ComponentData);

public:
	///////////////////////////////////////////////////////
	LightingComponentData();
	~LightingComponentData();
	///////////////////////////////////////////////////////
	ork::lev2::LightData*	GetLightData() const { return mLightData; }
	bool IsDynamic() const { return mbDynamic; }

private:
    ork::ent::ComponentInst *CreateComponent(ork::ent::Entity *pent) const final;

	void LdGetter(ork::rtti::ICastable*& val) const { val=mLightData; }
	void LdSetter(ork::rtti::ICastable* const & val) { mLightData=ork::rtti::downcast<ork::lev2::LightData*>(val); }
	void DoRegisterWithScene( ork::ent::SceneComposer& sc ) final;

	ork::lev2::LightData*	mLightData;
	bool					mbDynamic;
};

///////////////////////////////////////////////////////////////////////////////

class LightingComponentInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(LightingComponentInst, ork::ent::ComponentInst);

public:

	LightingComponentInst( const LightingComponentData &data, ork::ent::Entity *pent );

	ork::lev2::Light* GetLight() const { return mLight; }

private:

    ~LightingComponentInst() final;
	void DoUpdate(ork::ent::SceneInst *inst) final;
	bool DoLink(ork::ent::SceneInst *psi) final;

	ork::lev2::Light*	mLight;
	const LightingComponentData& mLightData;

};

///////////////////////////////////////////////////////////////////////////////

class LightArchetype : public Archetype
{
	RttiDeclareConcrete(LightArchetype, Archetype);
public:
	LightArchetype();
private:
	void DoCompose(ArchComposer& composer) final;  // virtual
	void DoStartEntity(SceneInst*, const CMatrix4& mtx, Entity* pent ) const final {}
};

///////////////////////////////////////////////////////////////////////////////

}}

