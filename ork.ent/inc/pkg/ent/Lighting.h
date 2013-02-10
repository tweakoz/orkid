////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_ENT_Lighting_H
#define ORK_ENT_Lighting_H

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/entity.h>
#include <ork/math/cvector3.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class LightingManagerComponentData : public ork::ent::SceneComponentData
{
	RttiDeclareConcrete(LightingManagerComponentData, ork::ent::SceneComponentData);

public:
	///////////////////////////////////////////////////////
	LightingManagerComponentData();
	ork::ent::SceneComponentInst* CreateComponentInst(ork::ent::SceneInst *pinst) const; // virtual 
	///////////////////////////////////////////////////////

	const ork::lev2::LightManagerData& Lmd() const { return mLmd; }

private:

	ork::Object* LmdAccessor() { return & mLmd; }

	ork::lev2::LightManagerData	mLmd;

};

///////////////////////////////////////////////////////////////////////////////

class LightingManagerComponentInst : public ork::ent::SceneComponentInst
{
	RttiDeclareAbstract(LightingManagerComponentInst, ork::ent::ComponentInst);

public:

	LightingManagerComponentInst( const LightingManagerComponentData &data, ork::ent::SceneInst *pinst );

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
	virtual ork::ent::ComponentInst *CreateComponent(ork::ent::Entity *pent) const;
	///////////////////////////////////////////////////////
	ork::lev2::LightData*	GetLightData() const { return mLightData; }
	bool IsDynamic() const { return mbDynamic; }

private:

	void LdGetter(ork::rtti::ICastable*& val) const { val=mLightData; }
	void LdSetter(ork::rtti::ICastable* const & val) { mLightData=ork::rtti::downcast<ork::lev2::LightData*>(val); }
	void DoRegisterWithScene( ork::ent::SceneComposer& sc );

	ork::lev2::LightData*	mLightData;
	bool					mbDynamic;
};

///////////////////////////////////////////////////////////////////////////////

class LightingComponentInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(LightingComponentInst, ork::ent::ComponentInst);

public:

	LightingComponentInst( const LightingComponentData &data, ork::ent::Entity *pent );
	~LightingComponentInst();

	ork::lev2::Light* GetLight() const { return mLight; }

private:

	virtual void DoUpdate(ork::ent::SceneInst *inst);
	/*virtual*/ bool DoLink(ork::ent::SceneInst *psi);

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
	void DoCompose(ArchComposer& composer); // virtual
	void DoStartEntity(SceneInst*, const CMatrix4& mtx, Entity* pent ) const {}
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
