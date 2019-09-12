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

	void DoStartEntity(Simulation* psi, const fmtx4 &world, Entity *pent ) const final {}
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
	fvec3	mEyeOffset;
	fvec3	mEyeUp;
	fvec3	mTgtOffset;
	float		mfAperature;
	float		mfNear;
	float		mfFar;
	
public:

	ent::ComponentInst* createComponent(ent::Entity* pent) const final;

	ObserverCamControllerData();
	PoolString GetTarget() const { return mTarget; }
	PoolString GetEye() const { return mEye; }
	fvec3 GetEyeUp() const { return mEyeUp; }
	fvec3 GetEyeOffset() const { return mEyeOffset; }
	fvec3 GetTgtOffset() const { return mTgtOffset; }
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
	CameraData								mCameraData;
	
	void DoUpdate(ent::Simulation* sinst) final;
    bool DoLink(Simulation *psi) final;
    bool DoStart(Simulation *psi, const fmtx4 &world) final;

public:
	const ObserverCamControllerData&	GetCD() const { return mCD; }

	ObserverCamControllerInst( const ObserverCamControllerData& cd, ork::ent::Entity* pent );
};

///////////////////////////////////////////////////////////////////////////////

} }
