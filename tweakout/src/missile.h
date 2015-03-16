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

	virtual ent::ComponentInst* CreateComponent(ent::Entity* pent) const;
	ork::ent::ArchetypeAsset* GetExplosionArchetype() const { return mpExplosionArchAsset; }

	MissileControllerData();
	ork::ent::ArchetypeAsset*	mpExplosionArchAsset;
};

///////////////////////////////////////////////////////////////////////////////

struct ITarget
{
	virtual void NotifyDamage(const CVector3& Impulse) = 0;
	virtual CVector3 GetPos() = 0;
};

///////////////////////////////////////////////////////////////////////////////

void LaunchMissile(	ent::SceneInst* sinst,
					const ent::DagNode& dn,
					const CVector3& InitialVelocity,
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
	CVector3						mWaypoint;
	CVector3						mLastTargetPos;
	float							mLifeTime;
	ITarget*						mTarget;
	float							mDamageMult;
	ent::RigidBody					mRigidBody;
	PIDController2<float>			mPIDController[3];

	CVector3 mPosition;
	CVector3 ZNormal;

	virtual void DoUpdate(ent::SceneInst* sinst);

	void CalcForces( float fddt );

	void Detonate(ork::ent::SceneInst *sinst,const ork::CMatrix4& mtx);

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

