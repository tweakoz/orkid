////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_ENT_OBSCAMERAARCH_H_
#define _ORK_ENT_OBSCAMERAARCH_H_

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

	/*virtual*/ void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const {}
	/*virtual*/ void DoCompose(ork::ent::ArchComposer& composer);

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
	CVector3	mTgtOffset;
	float		mfAperature;
	float		mfNear;
	float		mfFar;
	
public:

	virtual ent::ComponentInst* CreateComponent(ent::Entity* pent) const;

	ObserverCamControllerData();
	PoolString GetTarget() const { return mTarget; }
	PoolString GetEye() const { return mEye; }
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
	
	virtual void DoUpdate(ent::SceneInst* sinst);

public:
	const ObserverCamControllerData&	GetCD() const { return mCD; }

	ObserverCamControllerInst( const ObserverCamControllerData& cd, ork::ent::Entity* pent );
	bool DoLink(SceneInst *psi);
	bool DoStart(SceneInst *psi, const CMatrix4 &world);
};

///////////////////////////////////////////////////////////////////////////////

} }

#endif
