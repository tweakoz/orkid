////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/entity.h>
#include <pkg/ent/rigidbody.h>
#include <ork/math/PIDController.h>
#include <pkg/ent/ReferenceArchetype.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace terrain {
class heightfield_ed_inst;
}}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {
///////////////////////////////////////////////////////////////////////////////

class WorldControllerInst;

class MissileControllerData : public ent::ComponentData
{
	RttiDeclareAbstract( MissileControllerData, ent::ComponentData );

public:

	virtual ent::ComponentInst* createComponent(ent::Entity* pent) const final;
	ork::ent::ArchetypeAsset* GetExplosionArchetype() const { return mpExplosionArchAsset; }

	MissileControllerData();
	ork::ent::ArchetypeAsset*	mpExplosionArchAsset;
};

///////////////////////////////////////////////////////////////////////////////

struct ITarget
{
	virtual void NotifyDamage(const fvec3& Impulse) = 0;
	virtual fvec3 GetPos() = 0;
};

///////////////////////////////////////////////////////////////////////////////

void LaunchMissile(	ent::SceneInst* sinst,
					const ent::DagNode& dn,
					const fvec3& InitialVelocity,
					WorldControllerInst* wci,
					ITarget& tgt,
					float fdmgmult
					);

///////////////////////////////////////////////////////////////////////////////

class MissileControllerInst : public ent::ComponentInst
{
	//RttiDeclareAbstract( MissileControllerData, ent::ComponentData );
	//DECLARE_TRANSPARENT_ABSTRACT_RTTI( MissileControllerInst, ent::ComponentInst );

	enum ESTATE
	{
		ESTATE_RESET = 0,
		ESTATE_AQUIRE,
		ESTATE_SEEK,
		ESTATE_DETONATE
	};

	const MissileControllerData&	mCD;
	WorldControllerInst*			mWCI;
	ESTATE							meState;
	float							mftimer;
	fvec3						mWaypoint;
	fvec3						mLastTargetPos;
	float							mLifeTime;
	ITarget*						mTarget;
	float							mDamageMult;
	ent::RigidBody					mRigidBody;
	PIDController2<float>			mPIDController[3];

	fvec3 mPosition;
	fvec3 ZNormal;

	virtual void DoUpdate(ent::SceneInst* sinst) final;

	void CalcForces( float fddt );

	void Detonate(ork::ent::SceneInst *sinst,const ork::fmtx4& mtx);

public:

	void SetDamageMult( float dm ) { mDamageMult=dm; }

	void SetWCI( WorldControllerInst*wci ) { mWCI=wci; }

	MissileControllerInst( const MissileControllerData& pcd, ork::ent::Entity* pent );

	void SetTarget( ITarget* pt ) { mTarget=pt; }

	ent::RigidBody& RigidBody() { return mRigidBody; }
	const ent::RigidBody& RigidBody() const { return mRigidBody; }
};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
