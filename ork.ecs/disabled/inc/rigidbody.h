////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ENT_RIGIDBODY_H
#define _ENT_RIGIDBODY_H

#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/quaternion.h>

namespace ork { namespace ent {

#define _DEBUG_FORCES

struct PointMass
{
	fvec3	mPosition;

	///////////////////////////////
	fvec3	mWPosition;
	fvec3	mLastWPosition;
	///////////////////////////////

	float		mMass;
	fvec3	mMOI;	
	fvec3	mImpulse;
	fvec3	mColPos;
	fvec3	mColNrm;
	float		mCollisionDepth;
	float		mPointHeight;
	float		mLastPointHeight;
	float		msimp;

#if defined( _DEBUG_FORCES )
	orkvector<fvec3>	mForces;
#endif

   PointMass();
};

struct RigidBody
{
	////////////////////////////////////////
	// mass distribution variables
	////////////////////////////////////////

	orkvector<PointMass>    mPoints;
	fvec3                mCenterOfMass;
	float                   mTotalMass;

	fmtx4                mIniInertiaTensor;
	fmtx4                mIniInertiaTensorInv;
	fmtx4                mCurInertiaTensorInv;
	fmtx4                mCurInertiaTensor;

	////////////////////////////////////////
	// state vars
	////////////////////////////////////////

	fvec3                mPosition;
	fquat				mOrientation;
	fvec3				mAngularMomentum;
	fvec3				mLinearMomentum;
	
	////////////////////////////////////////
	// derived state vars
	////////////////////////////////////////

	fvec3                mVelocity;
	fvec3                mLinAccel;
	fvec3                mPrevLinAccel;
	fvec3                mPrevVelocity;
	fvec3				mAngularVelocity;
	fvec3                mAngAccel;

	fvec3                mLinImpulse;
	fvec3                mAngImpulse;

	fvec3                mTotalLinForce;
	fvec3                mTotalTorque;
	float                   mElasticity;

	////////////////////////////////////////

	fmtx4                mCurrentMatrix;
	fmtx4                mCurrentInvMatrix;

	////////////////////////////////////////

	void AddPointMass(const PointMass& pmass);

	void Close();


	RigidBody();

	////////////////////////////////////////////////////////
	// WorldSpace Functions

	fvec3 PointVelocityW( const fvec3& wp ) const;
	void BeginForces();
	void ApplyForce( const fvec3& Force, const fvec3& loc);
	void ApplyImpulse( const fvec3& Impulse, const fvec3& loc);
	void EndForces();

	fvec3 ComW() const;
	fvec3 PntW(int idx) const;

	static float DualBodyImpulse(   const fvec3& collisionnormal,
									const fvec3& collisionpoint,
									const RigidBody& rb1,
									const RigidBody& rb2,
									float rb1mass,
									float rb2mass  );

	static float SingleBodyImpulse(	const fvec3& collisionnormal,
									const fvec3& collisionpoint,
									const RigidBody& rb1,
									float rb1mass
									);

	////////////////////////////////////////////////////////

	void IntegrateForces( float fdt );
	void IntegrateImpulses( float fdt );

	void ComputeOrientation( float fdt );

	static fvec3 MassMOI_Box( const fvec3& whd, const float fmass );
	static fvec3 MassMOI_Sphere( const float radius, const float fmass );

};

} }

#endif
