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
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class ModelArchetype : public Archetype
{
	RttiDeclareConcrete( ModelArchetype, Archetype );

	void DoStartEntity(SceneInst* psi, const fmtx4 &world, Entity *pent ) const final {}
	void DoCompose(ork::ent::ArchComposer& composer) final;

public:

	ModelArchetype();

};

#if 0
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
	virtual ent::ComponentInst* createComponent(ent::Entity* pent) const;

	SkyBoxControllerData();
	float GetSpinRate() const { return mfSpinRate; }

};

///////////////////////////////////////////////////////////////////////////////

class SkyBoxControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( SkyBoxControllerInst, ent::ComponentInst );

	const SkyBoxControllerData&		mCD;
	float							mPhase;

	virtual void DoUpdate(ent::SceneInst* sinst);

public:
	const SkyBoxControllerData&	GetCD() const { return mCD; }

	SkyBoxControllerInst( const SkyBoxControllerData& cd, ork::ent::Entity* pent );
	float GetPhase() const { return mPhase; }
};

///////////////////////////////////////////////////////////////////////////////

class SkyBoxArchetype : public Archetype
{
	RttiDeclareConcrete( SkyBoxArchetype, Archetype );

	/*virtual*/ void DoLinkEntity( SceneInst* psi, Entity *pent ) const;
	/*virtual*/ void DoStartEntity(SceneInst* psi, const fmtx4 &world, Entity *pent ) const {}
	/*virtual*/ void DoCompose(ork::ent::ArchComposer& composer);

	//void					SetSkyTexture( ork::rtti::ICastable* const & l2tex);
	//void					GetSkyTexture( ork::rtti::ICastable* & l2tex) const;

	//lev2::TextureAsset *	mSkyTexture;
	//ork::lev2::EBlending	meBlendMode;
	//float					mfSpinRate;

public:

	//lev2::Texture*			SkyTexture() const { return (mSkyTexture==0) ? 0 : mSkyTexture->GetTexture(); }
	//ork::lev2::EBlending	GetBlendMode() const { return meBlendMode; }

	SkyBoxArchetype();
	//mutable lev2::GfxMaterial3DSolid* matsky;;
	//mutable lev2::GfxMaterial3DSolid* matpick;

};
#endif

///////////////////////////////////////////////////////////////////////////////

} }

