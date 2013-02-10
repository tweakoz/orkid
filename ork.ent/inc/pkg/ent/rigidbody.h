////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
	CVector3	mPosition;

	///////////////////////////////
	CVector3	mWPosition;
	CVector3	mLastWPosition;
	///////////////////////////////

	float		mMass;
	CVector3	mMOI;	
	CVector3	mImpulse;
	CVector3	mColPos;
	CVector3	mColNrm;
	float		mCollisionDepth;
	float		mPointHeight;
	float		mLastPointHeight;
	float		msimp;

#if defined( _DEBUG_FORCES )
	orkvector<CVector3>	mForces;
#endif

   PointMass();
};

struct RigidBody
{
	////////////////////////////////////////
	// mass distribution variables
	////////////////////////////////////////

	orkvector<PointMass>    mPoints;
	CVector3                mCenterOfMass;
	float                   mTotalMass;

	CMatrix4                mIniInertiaTensor;
	CMatrix4                mIniInertiaTensorInv;
	CMatrix4                mCurInertiaTensorInv;
	CMatrix4                mCurInertiaTensor;

	////////////////////////////////////////
	// state vars
	////////////////////////////////////////

	CVector3                mPosition;
	CQuaternion				mOrientation;
	CVector3				mAngularMomentum;
	CVector3				mLinearMomentum;
	
	////////////////////////////////////////
	// derived state vars
	////////////////////////////////////////

	CVector3                mVelocity;
	CVector3                mLinAccel;
	CVector3                mPrevLinAccel;
	CVector3                mPrevVelocity;
	CVector3				mAngularVelocity;
	CVector3                mAngAccel;

	CVector3                mLinImpulse;
	CVector3                mAngImpulse;

	CVector3                mTotalLinForce;
	CVector3                mTotalTorque;
	float                   mElasticity;

	////////////////////////////////////////

	CMatrix4                mCurrentMatrix;
	CMatrix4                mCurrentInvMatrix;

	////////////////////////////////////////

	void AddPointMass(const PointMass& pmass);

	void Close();


	RigidBody();

	////////////////////////////////////////////////////////
	// WorldSpace Functions

	CVector3 PointVelocityW( const CVector3& wp ) const;
	void BeginForces();
	void ApplyForce( const CVector3& Force, const CVector3& loc);
	void ApplyImpulse( const CVector3& Impulse, const CVector3& loc);
	void EndForces();

	CVector3 ComW() const;
	CVector3 PntW(int idx) const;

	static float DualBodyImpulse(   const CVector3& collisionnormal,
									const CVector3& collisionpoint,
									const RigidBody& rb1,
									const RigidBody& rb2,
									float rb1mass,
									float rb2mass  );

	static float SingleBodyImpulse(	const CVector3& collisionnormal,
									const CVector3& collisionpoint,
									const RigidBody& rb1,
									float rb1mass
									);

	////////////////////////////////////////////////////////

	void IntegrateForces( float fdt );
	void IntegrateImpulses( float fdt );

	void ComputeOrientation( float fdt );

	static CVector3 MassMOI_Box( const CVector3& whd, const float fmass );
	static CVector3 MassMOI_Sphere( const float radius, const float fmass );

};

} }

#endif
