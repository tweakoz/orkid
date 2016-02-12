////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/entity.h>
#include <pkg/ent/rigidbody.h>
#include "missile.h"
#include <pkg/ent/bullet.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace terrain {
class heightfield_ed_inst;
}}

namespace ork { namespace lev2 {
class InputState;
}}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {
///////////////////////////////////////////////////////////////////////////////

class WorldControllerInst;

class ShipControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( ShipControllerData, ent::ComponentData );

	ent::ComponentInst* DoCreateComponent(ent::Entity* pent) const final;

	float	mForwardForce;
	float	mSteeringRatio;
	float	mSteeringAngle;
	float	mGravity;
	float	mGroundFriction;
	float	mAirFriction;
	mutable float	mDT;
	float	mfFlipForce;
	int		mDebug;

public:

	float GetGroundFriction() const { return mGroundFriction; }
	float GetAirFriction() const { return mAirFriction; }


	float GetForwardForce() const { return mForwardForce; }
	float GetSteeringRatio() const { return mSteeringRatio; }
	float GetSteeringAngle() const { return mSteeringAngle; }
	float GetGravity() const { return mGravity; }
	float GetFlipForce() const { return mfFlipForce; }

	int  GetDebug() const { return mDebug; }
	float GetDT() const { return mDT; }
	void SetDT( float fv ) const { mDT=fv; }

	ShipControllerData();
};

class ShipControllerInst;

struct ShipTarget : public ITarget
{
	ShipControllerInst& mSCI;
	CVector3			mDamageImpulse;

	void NotifyDamage(const CVector3& Impulse) final;
	CVector3 GetPos() final;

	ShipTarget( ShipControllerInst& sci ) : mSCI(sci) {}
};

///////////////////////////////////////////////////////////////////////////////

class ShipControllerInst : public ent::ComponentInst
{
	//DECLARE_TRANSPARENT_ABSTRACT_RTTI( ShipControllerInst, ent::ComponentInst );

	const ShipControllerData&		mPcd;
	//ork::track::RailObject*			mRailObject;
	WorldControllerInst*			mWCI;

	//ent::RigidBody				mRigidBody;
	//ent::RigidBody				mGroundBody;
	btRigidBody*					mRigidBody;

	ShipTarget						mThisTarget;

	void DoUpdate(ent::SceneInst* sinst) final;
	bool DoLink(ent::SceneInst* psi) final;

	bool DoUpdate_Flip( const ork::lev2::InputState& inpstate, float fdt );

public:

	void SetWCI( WorldControllerInst*wci ) { mWCI=wci; }

	ShipControllerInst( const ShipControllerData& pcd, ent::Entity *entity );

	//void CreateRailObject( const ork::track::Rail& rail );
	//const ork::track::RailObject* RailObject() const { return mRailObject; }

	//ent::RigidBody& RigidBody() { return mRigidBody; }
	//const ent::RigidBody& RigidBody() const { return mRigidBody; }

	void LaunchMissile(ent::SceneInst* sinst);

	bool Collision(float dt);

	const ShipControllerData& GetSCD() const { return mPcd; }
	ITarget& GetITarget() { return mThisTarget; }


};

///////////////////////////////////////////////////////////////////////////////

class ShipArchetype : public ork::ent::Archetype
{
	RttiDeclareConcrete( ShipArchetype, ent::Archetype );
		
	void DoCompose(ork::ent::ArchComposer& composer) final;
	void DoStartEntity( ent::SceneInst* psi, const ork::CMatrix4& mtx, ent::Entity *pent ) const final;

public:

	ShipArchetype();

};

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////

