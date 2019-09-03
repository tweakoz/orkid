///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/rigidbody.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

PointMass::PointMass()
   : mMass(1.0f)
   , mCollisionDepth(0.0f)
{
#if defined( _DEBUG_FORCES )
	mForces.reserve(10);
#endif
}

#define setelemGL SetElemXY
#define setelemNGL SetElemYX

///////////////////////////////////////////////////////////////////////////////

RigidBody::RigidBody()
   : mTotalMass( 0.0f )
   , mElasticity(0.5f)
   , mAngAccel( 0.0f, 0.0f, 0.0f )
   , mLinAccel( 0.0f, 0.0f, 0.0f )
   , mPosition( 0.0f, 0.0f, 0.0f )
   , mVelocity( 0.0f, 0.0f, 0.0f )
   , mAngularMomentum(0.0f, 0.0f, 0.0f)
   , mLinearMomentum(0.0f,0.0f,0.0f)
   , mAngularVelocity( 0.0f, 0.0f, 0.0f )
   , mLinImpulse( 0.0f, 0.0f, 0.0f )
   , mAngImpulse( 0.0f, 0.0f, 0.0f )
   , mTotalLinForce( 0.0f, 0.0f, 0.0f )
   , mTotalTorque( 0.0f, 0.0f, 0.0f )

{
   for( int i=0; i<4; i++ )
           for( int j=0; j<4; j++ )
               mIniInertiaTensor.setelemGL(i,j,0.0f);
   mIniInertiaTensor.setelemGL(3,3,1.0f);
}

///////////////////////////////////////////////////////////////////////////////
// generate a matrix that will transform a vector to be the cross-product of "in"
///////////////////////////////////////////////////////////////////////////////

void SkewSymmetric( const fvec3& in, fmtx4& out )
{
   ///////////////////////////////////
   // set main elements
   ///////////////////////////////////

   out.setelemNGL( 0, 0,    0.0f );
   out.setelemNGL( 0, 1,    in.GetZ() );
   out.setelemNGL( 0, 2,    -in.GetY() );

   out.setelemNGL( 1, 0,    -in.GetZ() );
   out.setelemNGL( 1, 1,    0.0f );
   out.setelemNGL( 1, 2,    in.GetX() );

   out.setelemNGL( 2, 0,    in.GetY() );
   out.setelemNGL( 2, 1,    -in.GetX() );
   out.setelemNGL( 2, 2,    0.0f );

   ///////////////////////////////////
   // just for completeness
   ///////////////////////////////////

   out.setelemNGL( 3, 0,    0.0f );
   out.setelemNGL( 3, 1,    0.0f );
   out.setelemNGL( 3, 2,    0.0f );

   out.setelemNGL( 0, 3,    0.0f );
   out.setelemNGL( 1, 3,    0.0f );
   out.setelemNGL( 2, 3,    0.0f );
   out.setelemNGL( 3, 3,    1.0f );

   ///////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::AddPointMass(const PointMass& pmass)
{
   mPoints.push_back(pmass);
}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::Close()
{
   int inump = int(mPoints.size());

   //////////////////
   // calc COM
   //////////////////

   mTotalMass = 0.0f;
   mCenterOfMass = fvec3(0.0f,0.0f,0.0f);
   for( int i=0; i<inump; i++ )
   {
       const PointMass& pm = mPoints[i];
       mTotalMass += pm.mMass;
       mCenterOfMass += (pm.mPosition*pm.mMass);

   }
   mCenterOfMass = mCenterOfMass*(1.0f/mTotalMass);

   //////////////////
   // calc InertiaTensor at CenterOfMass
   //////////////////

	float ixx=0.0f;
	float iyy=0.0f;
	float izz=0.0f;
	float ixy=0.0f;
	float ixz=0.0f;
	float iyz=0.0f;

	for( int i=0; i<inump; i++ )
	{
		const PointMass& pm = mPoints[i];
		const fvec3 r = (pm.mPosition-mCenterOfMass);
		const float rx = r.GetX();
		const float ry = r.GetY();
		const float rz = r.GetZ();
		const fvec3 li = pm.mMOI;

		ixx += li.GetX() + pm.mMass*(ry*ry+rz*rz);
		iyy += li.GetY() + pm.mMass*(rx*rx+rz*rz);
		izz += li.GetZ() + pm.mMass*(rx*rx+ry*ry);

		ixy += pm.mMass*(rx*ry);
		ixz += pm.mMass*(rx*rz);
		iyz += pm.mMass*(ry*rz);
	}

	mIniInertiaTensor = fmtx4::Identity;

	mIniInertiaTensor.setelemGL( 0,0, +ixx );
	mIniInertiaTensor.setelemGL( 0,1, -ixy );
	mIniInertiaTensor.setelemGL( 0,2, -ixz );

	mIniInertiaTensor.setelemGL( 1,0, -ixy );
	mIniInertiaTensor.setelemGL( 1,1, +iyy );
	mIniInertiaTensor.setelemGL( 1,2, -iyz );

	mIniInertiaTensor.setelemGL( 2,0, -ixz );
	mIniInertiaTensor.setelemGL( 2,1, -iyz );
	mIniInertiaTensor.setelemGL( 2,2, +izz );

	mIniInertiaTensor.setelemGL( 3,3, 1.0f );

	//////////////////////////////////////////////
	// initial inverse tensors
	//////////////////////////////////////////////
	mCurInertiaTensor = mIniInertiaTensor;
	mCurInertiaTensorInv.inverseOf( mCurInertiaTensor );
	mIniInertiaTensorInv = mCurInertiaTensorInv;
	//////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::BeginForces()
{
	mTotalLinForce = fvec3(0.0f,0.0f,0.0f);
	mTotalTorque = fvec3(0.0f,0.0f,0.0f);
	mAngAccel = fvec3(0.0f,0.0f,0.0f);


	mLinAccel = fvec3(0.0f,0.0f,0.0f);

	int inump = int(mPoints.size());
	for( int i=0; i<inump; i++ )
	{
		PointMass& pm = mPoints[i];
#if defined( _DEBUG_FORCES )
		pm.mForces.clear();
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::ApplyImpulse( const fvec3& Impulse, const fvec3& loc )
{
	///////////////////////////////
	// world to obj space xf
	///////////////////////////////

	fvec3 ObjImpulse = Impulse.Transform3x3( mCurrentInvMatrix );
	fvec3 ObjLoc = fvec4( loc, 1.0f ).Transform( mCurrentInvMatrix ).xyz();
	float ObjDist = ObjLoc.Mag();

	///////////////////////////////
	const fvec3 rad = (ObjLoc-mCenterOfMass);	// Meters
	fvec3 Moment = rad.Cross(ObjImpulse);		// NewtonMeters
	mAngImpulse += Moment;
	mLinImpulse += Impulse;

	///////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::ApplyForce( const fvec3& Force, const fvec3& loc )
{
   const fvec3 rad = (loc-ComW());
   fvec3 Torque = rad.Cross(Force);		// NewtonsMeters
   mTotalLinForce += Force;
   mTotalTorque += Torque;

#if defined( _DEBUG_FORCES )
   int inump = int(mPoints.size());
   for( int i=0; i<inump; i++ )
   {
	   if( (loc-PntW(i)).MagSquared()<0.01f )
	   {
			mPoints[i].mForces.push_back(Force);
	   }
   }
#endif
}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::EndForces()
{
}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::IntegrateForces( float fdt )
{
	/////////////////////
	// mAngularMomentum is in object space
	/////////////////////

	fvec3 dam_t = mTotalTorque.Transform3x3(mCurrentInvMatrix)*fdt;

	//orkprintf( "dam_t <%+02.4f %+02.4f %+02.4f> cam <%+02.4f %+02.4f %+02.4f>\r",
				//dam_t.GetX(), dam_t.GetY(), dam_t.GetZ(),
				//mAngularMomentum.GetX(), mAngularMomentum.GetY(), mAngularMomentum.GetZ() );

	mAngularMomentum += dam_t;

	///////////////////
	// linear
	///////////////////

	mLinearMomentum += (mTotalLinForce*fdt);
}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::IntegrateImpulses( float fdt )
{
	//////////////////////////////////////////////////////////////////
	// impulses
	//////////////////////////////////////////////////////////////////

	mLinearMomentum += mLinImpulse;
	mAngularMomentum += mAngImpulse;
	mLinImpulse = fvec3(0.0f,0.0f,0.0f);
	mAngImpulse = fvec3(0.0f,0.0f,0.0f);

	//////////////////////////////////////////////////////////////////
	// update velocity
	//////////////////////////////////////////////////////////////////

	mPrevVelocity = mVelocity;

	fvec3 amw = mAngularMomentum.Transform3x3( mCurrentMatrix );
	mAngularVelocity = amw.Transform( mCurInertiaTensorInv );
	mVelocity = mLinearMomentum*(1.0f/mTotalMass);

	//////////////////////////////////////////////////////////////////
	// update position / orientation
	//////////////////////////////////////////////////////////////////

	fvec3 av_axis = mAngularVelocity.Normal();
	float av_mag = mAngularVelocity.Mag();

	fquat qav; qav.FromAxisAngle( fvec4( av_axis, av_mag*fdt ) );
	qav.Normalize();

	mOrientation = mOrientation.Multiply(qav);
	mOrientation.Normalize();

	ComputeOrientation(fdt);

	//////////////////////////////////////////////////////////////////
	// y[i+1] = y[i] + (h/2) * (f(t[i],y[i])+f(t[i]+h,y[i]+h*f(t[i],y[i]))
	// y[i+1] = y[i] + h*f(t[i],y[i])
	// y[i+1] = y[i] + t*dydt
	//////////////////////////////////////////////////////////////////

	mPosition += mVelocity*fdt;
	//mPosition += (fdt*0.5f) * (mVelocity+fdt*(mPrevVelocity-mVelocity));


   int inump = int(mPoints.size());
   for( int i=0; i<inump; i++ )
   {
		mPoints[i].mLastWPosition = mPoints[i].mWPosition;
		mPoints[i].mWPosition = PntW(i);
   }

	mLinImpulse = fvec3( 0.0f,0.0f,0.0f);
	mAngImpulse = fvec3( 0.0f,0.0f,0.0f);

}

///////////////////////////////////////////////////////////////////////////////

void RigidBody::ComputeOrientation( float fdt )
{
	///////////////////
	// To Matrix
	///////////////////

	fmtx4 MatRot;

	MatRot = mOrientation.ToMatrix();

	mCurrentMatrix = MatRot;
	mCurrentMatrix.SetTranslation( mPosition );

	mCurrentInvMatrix.inverseOf( mCurrentMatrix );

	//////////////////////////////////////////////////////
	// update inertia tensors

	fmtx4 MatOT = MatRot;
	MatOT.Transpose();
	mCurInertiaTensorInv = MatOT*(mIniInertiaTensorInv * MatRot);
	mCurInertiaTensor.inverseOf( mCurInertiaTensorInv );

	//////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

fvec3 RigidBody::ComW() const
{
   return fvec4( mCenterOfMass, 1.0f ).Transform( mCurrentMatrix ).xyz();
}

///////////////////////////////////////////////////////////////////////////////

fvec3 RigidBody::PntW(int idx) const
{
   return fvec4( mPoints[idx].mPosition, 1.0f ).Transform( mCurrentMatrix ).xyz();
}

///////////////////////////////////////////////////////////////////////////////

fvec3 RigidBody::PointVelocityW( const fvec3& wp ) const
{
	fvec3 delta = (wp-ComW());
	fvec3 vel = mVelocity + mAngularVelocity.Cross(delta);
	return vel;
}

///////////////////////////////////////////////////////////////////////////////

float RigidBody::SingleBodyImpulse(	const fvec3& cn, // collision normal (world space)
									const fvec3& cp,	// collision point  (world space)
									const RigidBody& rb1,
									float rb1mass )
{
	const float felas = rb1.mElasticity;
	////////////////////////////////////////////////
	fvec3 r = (cp - rb1.ComW());
	fvec3 pv = rb1.PointVelocityW(cp);
	fvec3 v = pv-(cn*0.5f);
	////////////////////////////////////////////////
	float v_dot_cn				= v.Dot( cn );
	float ImpulseNumerator		= -v_dot_cn * (1.0f + felas);
	////////////////////////////////////////////////
	float		ndnm				= (1.0f/rb1mass)
									* cn.Dot(cn);
	fvec3	rxn					= r.Cross(cn);
	fvec3	rxni				= rxn.Transform3x3( rb1.mCurInertiaTensorInv );
	fvec3	t2					= rxni.Cross(r);

	float ImpulseDenominator	= ndnm
								+ t2.Dot(cn);
	////////////////////////////////////////////////
	float impulse = (ImpulseNumerator/ImpulseDenominator);
	return impulse;
}

///////////////////////////////////////////////////////////////////////////////

float RigidBody::DualBodyImpulse(	const fvec3& cn,
									const fvec3& cp,
									const RigidBody& rb1,
									const RigidBody& rb2,
									float rb1mass,
									float rb2mass )
{
	const float felas = (rb1.mElasticity+rb2.mElasticity)*0.5f;
	////////////////////////////////////////////////
	fvec3 r1 = (cp - rb1.ComW());
	fvec3 r2 = (cp - rb2.ComW());
	fvec3 pv1 = rb1.PointVelocityW(cp);
	fvec3 pv2 = rb2.PointVelocityW(cp);
	fvec3 pv = pv1-pv2;
	fvec3 v = pv-(cn*0.5f);
	////////////////////////////////////////////////
	float v_dot_cn					= v.Dot( cn );
	float ImpulseNumerator			= -v_dot_cn * (1.0f + felas);
	////////////////////////////////////////////////
	float		ndnm				= ((1.0f/rb1mass)+(1.0f/rb2mass))
									* cn.Dot(cn);
	////////////////////////////////////////////////
	fvec3	rxn1				= r1.Cross(cn);
	fvec3	rxni1				= rxn1.Transform3x3( rb1.mCurInertiaTensorInv );
	fvec3	t21					= rxni1.Cross(r1);
	////////////////////////////////////////////////
	fvec3	rxn2				= r1.Cross(cn);
	fvec3	rxni2				= rxn2.Transform3x3( rb2.mCurInertiaTensorInv );
	fvec3	t22					= rxni2.Cross(r2);
	////////////////////////////////////////////////

	float ImpulseDenominator	= ndnm
								+ (t21+t22).Dot(cn);
	////////////////////////////////////////////////
	float impulse = (ImpulseNumerator/ImpulseDenominator);














	return impulse;
}

///////////////////////////////////////////////////////////////////////////////
// calculate Mass Moment of Inertia for an axis aligned box
///////////////////////////////////////////////////////////////////////////////

fvec3 RigidBody::MassMOI_Box( const fvec3& whd, const float fmass )
{
	fvec3 rval;
	float xsq = whd.GetX()*whd.GetX();
	float ysq = whd.GetY()*whd.GetY();
	float zsq = whd.GetZ()*whd.GetZ();
	rval.SetX( (1.0f/12.0f)*fmass*(ysq+zsq) );
	rval.SetY( (1.0f/12.0f)*fmass*(xsq+zsq) );
	rval.SetZ( (1.0f/12.0f)*fmass*(xsq+ysq) );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
// calculate Mass Moment of Inertia for a sphere
///////////////////////////////////////////////////////////////////////////////

fvec3 RigidBody::MassMOI_Sphere( const float fradius, const float fmass )
{
	fvec3 rval;
	float fv = (2.0f/5.0f)*fmass*(fradius*fradius);
	rval.SetXYZ( fv, fv, fv );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
// Basic Rigid Body Draw Routine
///////////////////////////////////////////////////////////////////////////////

void RigidBody_Draw( lev2::GfxTarget* targ, const fmtx4& matw, const RigidBody& rbody, bool bdebug )
{
	fmtx4 MatS;
	static lev2::GfxMaterial3DSolid matsolid(targ);

	///////////////////////////////////////////////////////
	// Basic Draw
	///////////////////////////////////////////////////////

	targ->BindMaterial( & matsolid );
	for( int i=0; i<int(rbody.mPoints.size()); i++ )
	{
		const ent::PointMass& pm = rbody.mPoints[i];
		const fvec4 a = rbody.PntW( i );

		////////////////////////////////////////////////
		// Draw Point Mass
		////////////////////////////////////////////////
		MatS.SetScale( 0.1f, 0.1f, 0.1f );
		MatS.SetTranslation( pm.mPosition );
		targ->PushModColor( fvec3::White() );
		targ->MTXI()->PushMMatrix( MatS*matw );
		{
			matsolid.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
			lev2::GfxPrimitives::GetRef().RenderDiamond( targ );

		}
		targ->PopModColor();
		targ->MTXI()->PopMMatrix();

		////////////////////////////////////////////////
		// Draw Collision Point
		////////////////////////////////////////////////
		MatS.SetScale( 0.05f, 0.05f, 0.05f );
		MatS.SetTranslation( pm.mColPos );
		targ->PushModColor( fvec3::MediumGrey() );
		targ->MTXI()->PushMMatrix( MatS );
		{
			matsolid.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
			lev2::GfxPrimitives::GetRef().RenderDiamond( targ );

		}
		targ->PopModColor();
		targ->MTXI()->PopMMatrix();
	}

	///////////////////////////////////////////////////////
	// Debug Draw
	///////////////////////////////////////////////////////

	if( bdebug )
	{
		targ->MTXI()->PushMMatrix( fmtx4::Identity );
		{
			for( int i=0; i<int(rbody.mPoints.size()); i++ )
			{
				const ent::PointMass& pm = rbody.mPoints[i];
				const fvec4 a = rbody.PntW( i );
				////////////////////////////////////////////////
				// Draw Point Impulse
				////////////////////////////////////////////////
				targ->PushModColor( fvec3::Red() );
				{
					fvec4 a2 = pm.mColPos;
					fvec4 b = a2+(pm.mImpulse);

					//targ->ImmDrawLine( a2, b );
				}
				targ->PopModColor();
				if( pm.mImpulse.Mag()<0.1f )
				{
					targ->PushModColor( (pm.msimp<0.0f) ? fvec3::Blue() : fvec3::Green() );
					{
						fvec4 a2 = pm.mColPos+pm.mImpulse;
						fvec4 b = pm.mColPos+pm.mImpulse.Normal();
						//targ->ImmDrawLine( a2, b );
					}
					targ->PopModColor();
				}

				////////////////////////////////////////////////
				// Draw Calculated Point Velocity
				////////////////////////////////////////////////
				targ->PushModColor( fvec3::White() );
				{
					fvec4 b = a+rbody.PointVelocityW( a );
					//targ->ImmDrawLine( a, b );
				}
				targ->PopModColor();

#if defined( _DEBUG_FORCES )
				////////////////////////////////////////////////
				// Draw Point Forces
				////////////////////////////////////////////////

				int inumf = pm.mForces.size();

				targ->PushModColor( fvec3::Yellow() );
				for( int ifo=0; ifo<inumf; ifo++ )
				{
					const fvec3& fc = pm.mForces[ifo];
					fvec4 b = a+fc;
					//targ->ImmDrawLine( a, b );

				}
				targ->PopModColor();

#endif
			}

			////////////////////////////////////////////////
			// Draw Angular Velocity
			////////////////////////////////////////////////

			targ->PushModColor( fvec3::Magenta() );
			{
				fvec3 a2 =rbody.mCenterOfMass;
				fvec4 b = a2+rbody.mAngularVelocity;
				//targ->ImmDrawLine( a2, b );
			}
			targ->PopModColor();

		}
		targ->MTXI()->PopMMatrix();
	}

}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
