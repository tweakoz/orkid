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

class ObserverCamArchetype : public Archetype
{
	RttiDeclareConcrete( ObserverCamArchetype, Archetype );

	void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const final {}
	void DoCompose(ork::ent::ArchComposer& composer) final;

public:

	ObserverCamArchetype();

};

///////////////////////////////////////////////////////////////////////////////

class ObserverCamControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( ObserverCamControllerData, ent::ComponentData );

	PoolString	mEye;
	PoolString	mTarget;
	CVector3	mEyeOffset;
	CVector3	mEyeUp;
	CVector3	mTgtOffset;
	float		mfAperature;
	float		mfNear;
	float		mfFar;
	
	ent::ComponentInst* DoCreateComponent(ent::Entity* pent) const final;

public:

	ObserverCamControllerData();
	PoolString GetTarget() const { return mTarget; }
	PoolString GetEye() const { return mEye; }
	CVector3 GetEyeUp() const { return mEyeUp; }
	CVector3 GetEyeOffset() const { return mEyeOffset; }
	CVector3 GetTgtOffset() const { return mTgtOffset; }
	float GetAperature() const { return mfAperature; }
	float GetNear() const { return mfNear; }
	float GetFar() const { return mfFar; }

};

///////////////////////////////////////////////////////////////////////////////

class ObserverCamControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( ObserverCamControllerInst, ent::ComponentInst );

	const ObserverCamControllerData&		mCD;
	Entity*									mpEye;
	Entity*									mpTarget;
	CCameraData								mCameraData;
	
	void DoUpdate(ent::SceneInst* sinst) final;
	bool DoLink(SceneInst *psi) final;
	bool DoStart(SceneInst *psi, const CMatrix4 &world) final;

public:
	const ObserverCamControllerData&	GetCD() const { return mCD; }

	ObserverCamControllerInst( const ObserverCamControllerData& cd, ork::ent::Entity* pent );
};

///////////////////////////////////////////////////////////////////////////////

} }
