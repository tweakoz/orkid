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
#include <ork/gfx/camera.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class TetherCamArchetype : public Archetype
{
	RttiDeclareConcrete( TetherCamArchetype, Archetype );

	void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const override {}
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
	CVector3	mEyeUp;
	CVector3	mEyeOffset;
	CVector3	mTgtOffset;
	float		mfAperature;
	float		mfNear;
	float		mfFar;
	float 		mApproachSpeed;
	
public:

	ent::ComponentInst* CreateComponent(ent::Entity* pent) const override;

	TetherCamControllerData();
	PoolString GetTarget() const { return mTarget; }
	CVector3 GetEyeUp() const { return mEyeUp; }
	CVector3 GetEyeOffset() const { return mEyeOffset; }
	CVector3 GetMinDistance() const { return mMinDistance; }
	CVector3 GetMaxDistance() const { return mMaxDistance; }
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
	Entity*									mpTarget;
	CCameraData								mCameraData;
	
	virtual void DoUpdate(ent::SceneInst* sinst);

public:
	const TetherCamControllerData&	GetCD() const { return mCD; }

	TetherCamControllerInst( const TetherCamControllerData& cd, ork::ent::Entity* pent );
	bool DoLink(SceneInst *psi) override;
	bool DoStart(SceneInst *psi, const CMatrix4 &world) override;
};

///////////////////////////////////////////////////////////////////////////////

} }
