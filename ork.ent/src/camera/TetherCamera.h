////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/camera/cameradata.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class TetherCamArchetype : public Archetype
{
	RttiDeclareConcrete( TetherCamArchetype, Archetype );

	void DoStartEntity(Simulation* psi, const fmtx4 &world, Entity *pent ) const override {}
	void DoCompose(ork::ent::ArchComposer& composer) override;

public:

	TetherCamArchetype();

};

///////////////////////////////////////////////////////////////////////////////

class TetherCamControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( TetherCamControllerData, ent::ComponentData );

	PoolString	mTarget;
	float		mMinDistance;
	float		mMaxDistance;
	fvec3	mEyeUp;
	fvec3	mEyeOffset;
	fvec3	mTgtOffset;
	float		mfAperature;
	float		mfNear;
	float		mfFar;
	float 		mApproachSpeed;

public:

	ent::ComponentInst* createComponent(ent::Entity* pent) const override;

	TetherCamControllerData();
	PoolString GetTarget() const { return mTarget; }
	fvec3 GetEyeUp() const { return mEyeUp; }
	fvec3 GetEyeOffset() const { return mEyeOffset; }
	fvec3 GetMinDistance() const { return mMinDistance; }
	fvec3 GetMaxDistance() const { return mMaxDistance; }
	float GetAperature() const { return mfAperature; }
	float GetNear() const { return mfNear; }
	float GetFar() const { return mfFar; }
	float GetApproachSpeed() const { return mApproachSpeed; }

};

///////////////////////////////////////////////////////////////////////////////

class TetherCamControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( TetherCamControllerInst, ent::ComponentInst );

	const TetherCamControllerData&			mCD;
	Entity*									mpTarget = nullptr;
	lev2::CameraData*				_cameraData = nullptr;

	void DoUpdate(ent::Simulation* sinst) final;
    bool DoLink(Simulation *psi) final;
    bool DoStart(Simulation *psi, const fmtx4 &world) final;

public:
	const TetherCamControllerData&	GetCD() const { return mCD; }

	TetherCamControllerInst( const TetherCamControllerData& cd, ork::ent::Entity* pent );
  ~TetherCamControllerInst();
};

///////////////////////////////////////////////////////////////////////////////

} }
