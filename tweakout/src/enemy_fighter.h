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
#include "missile.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace terrain {
class heightfield_ed_inst;
}}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {
///////////////////////////////////////////////////////////////////////////////

class WorldControllerInst;

class FighterControllerData : public ent::ComponentData
{
	RttiDeclareAbstract( FighterControllerData, ent::ComponentData );

public:

	virtual ent::ComponentInst* CreateComponent(ent::Entity* pent) const;

	FighterControllerData();
};

class FighterControllerInst;

struct FighterTarget : public ITarget
{
	CVector3	mDamageImpulse;
	FighterControllerInst& mFCI;
	/*virtual*/ void NotifyDamage(const CVector3& Impulse);
	/*virtual*/ CVector3 GetPos();
	FighterTarget( FighterControllerInst& fci ) : mFCI(fci) {}

};

///////////////////////////////////////////////////////////////////////////////

struct HotSpot;

class EnemySpawnerControllerInst;

class FighterControllerInst : public ent::ComponentInst
{
	//DECLARE_TRANSPARENT_ABSTRACT_RTTI( FighterControllerInst, ent::ComponentInst );

	enum ESTATE
	{
		ESTATE_RESET = 0,		
		ESTATE_PLACEMENT,		
		ESTATE_DODGE,		
		ESTATE_ATTACK,		
	};

	const FighterControllerData&	mCD;
	ent::Entity*					mTarget;
	WorldControllerInst*			mWCI;
	EnemySpawnerControllerInst*		mSpawner;
	ESTATE							meState;
	float							mLifeTime;
	float							mfRePosTimer;
	float							mfReAssTimer;
	float							mfMissileTimer;
	CVector3						mWaypoint;
	CVector3						mVelocity;
	FighterTarget					mThisTarget;
	float							mHitPoints;
	HotSpot*						mHotSpot;

	ent::RigidBody				mRigidBody;
	PIDController2<float>			mPositionController[3];

	CVector3 mPosition;
	CVector3 ZNormal;

	virtual void DoUpdate(ent::SceneInst* sinst);

	void CalcForces( float fddt );

	void DespawnSelf(ork::ent::SceneInst *sinst);

public:

	void SetWCI( WorldControllerInst*wci ) { mWCI=wci; }

	FighterControllerInst( const FighterControllerData& cd, ork::ent::Entity* pent );

	void SetTarget( ent::Entity*pent ) { mTarget=pent; }
	void setSpawner( EnemySpawnerControllerInst* pspw ) { mSpawner=pspw; }
	void SetHotSpot( HotSpot* hs );
	ent::RigidBody& RigidBody() { return mRigidBody; }
	const ent::RigidBody& RigidBody() const { return mRigidBody; }

	ITarget& GetITarget() { return mThisTarget; }

	void Damage( float hp );

	static int NumEnemies;
};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////

