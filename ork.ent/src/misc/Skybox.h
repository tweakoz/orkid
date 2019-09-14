////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/gfx/camera.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class SkyBoxControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( SkyBoxControllerData, ent::ComponentData );

	float mfSpinRate;

	void GetModelAccessor(ork::rtti::ICastable *&model) const;
	void SetModelAccessor(ork::rtti::ICastable *const &model);
	lev2::XgmModelAsset*					mModelAsset;
	float									mfScale;

public:

	lev2::XgmModel* GetModel() const;
	float GetScale() const { return mfScale; }
	ent::ComponentInst* createComponent(ent::Entity* pent) const final;

	SkyBoxControllerData();
	float GetSpinRate() const { return mfSpinRate; }

};

///////////////////////////////////////////////////////////////////////////////

class SkyBoxControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( SkyBoxControllerInst, ent::ComponentInst );

	const SkyBoxControllerData&		mCD;
	float							mPhase;

	void DoUpdate(ent::Simulation* sinst) final;

public:
	const SkyBoxControllerData&	GetCD() const { return mCD; }

	SkyBoxControllerInst( const SkyBoxControllerData& cd, ork::ent::Entity* pent );
	float GetPhase() const { return mPhase; }
};

///////////////////////////////////////////////////////////////////////////////

class SkyBoxArchetype : public Archetype
{
	RttiDeclareConcrete( SkyBoxArchetype, Archetype );

	void DoLinkEntity( Simulation* psi, Entity *pent ) const final;
	void DoStartEntity(Simulation* psi, const fmtx4 &world, Entity *pent ) const final {}
	void DoCompose(ork::ent::ArchComposer& composer) final;


public:

	SkyBoxArchetype();

};

///////////////////////////////////////////////////////////////////////////////

} }

