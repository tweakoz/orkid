////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <pkg/ent/ModelArchetype.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include "enemy_fighter.h"
#include "enemy_spawner.h"
#include "world.h"
#include "ship.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::FighterControllerData, "FighterControllerData" );
//INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::FighterControllerInst, "FighterControllerInst" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { 
namespace ent { void RigidBody_Draw( lev2::GfxTarget* targ, const fmtx4& matw, const RigidBody& rbody, bool bdebug ); }
namespace wiidom {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FighterControllerData::Describe()
{
	ork::ent::RegisterFamily<FighterControllerData>(ork::AddPooledLiteral("control"));
}

FighterControllerData::FighterControllerData()
{
}

ent::ComponentInst* FighterControllerData::createComponent(ent::Entity* pent) const
{
	return OrkNew FighterControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

fvec3 FighterTarget::GetPos()
{
	return mFCI.RigidBody().mPosition;
}
void FighterTarget::NotifyDamage(const fvec3& Impulse)
{
	mDamageImpulse = Impulse;
	mFCI.Damage( 1.0f );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

int FighterControllerInst::NumEnemies = 0;

void FighterControllerInst::Damage( float hp )
{
	mHitPoints -= hp;
}

//void FighterControllerInst::Describe()
//{
//}

static const float kinitlife = 30.0f;
static const float kassignmenttime = 3.0f;

FighterControllerInst::FighterControllerInst( const FighterControllerData& pcd, ork::ent::Entity* pent )
	: ork::ent::ComponentInst( & pcd, pent )
	, mCD(pcd)
	, mTarget( 0 )
	, meState( ESTATE_RESET )
	, mSpawner( 0 )
	, mThisTarget(*this)
	, mHitPoints( 2.0f )
	, mHotSpot(0)
	////////////////////////////////////
	, mfRePosTimer( 0.0f )
	, mfReAssTimer( kassignmenttime )
	, mfMissileTimer( 0.0f )
	, mLifeTime( kinitlife )
	////////////////////////////////////
{
	////////////////////////////////////
	// fighter's rigid body
	////////////////////////////////////

	ent::PointMass p0;
	const float pmass = 20000.0f;
	p0.mMass = pmass;
	p0.mMOI = ent::RigidBody::MassMOI_Sphere( 0.1f, p0.mMass );
	const float frad = 3.0f;
	const float cele = 0.0f;
	p0.mPosition.SetXYZ( -frad, cele, -frad );
	mRigidBody.AddPointMass( p0 );

	mRigidBody.Close();

	mRigidBody.mPosition = fvec3( 0.0f, 200.0f, 0.0f );
	
	mRigidBody.BeginForces();
	mRigidBody.EndForces();
	mRigidBody.IntegrateForces(0.0f);
	mRigidBody.mElasticity = 0.9f;


	////////////////////////////////////
	// PID controllers
	////////////////////////////////////
	
	static const float kprop = 0.9f;	// proportion per update to approach target
	static const float kintg = 0.0f;	// mystery value
	static const float kderi = 0.04f;	// minimum approach velocity

	static const fvec2 pidrange(-100.0f,100.0f);
	static const fvec2 pidmaxd(-30.0f,30.0f);
	mPositionController[0].Configure( kprop, kintg, kderi, pidrange, pidmaxd );
	mPositionController[1].Configure( kprop, kintg, kderi, pidrange, pidmaxd );
	mPositionController[2].Configure( kprop, kintg, kderi, pidrange, pidmaxd );

	//////////////////////////////////////////////////////////////
	// init the entities drawable to this callback
	//////////////////////////////////////////////////////////////
	struct yo
	{
		static void doit( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren )
		{
			return; //
			///////////////////////
			// Draw RigidBody Object
			///////////////////////
			/*const ent::Entity* pent = (const ent::Entity*) pren->GetUserData();
			const FighterControllerInst* sci = pent->GetTypedComponent<FighterControllerInst>();
			OrkAssert( sci );
			FighterControllerInst* scinc = const_cast<FighterControllerInst*>(sci);
			fmtx4 MatS;
			static lev2::GfxMaterial3DSolid matsolid(targ);

			///////////////////////////////////////////////////////
			// Basic Draw
			///////////////////////////////////////////////////////

			const ent::RigidBody& rbody = sci->RigidBody();

			targ->BindMaterial( & matsolid );
			//const ent::PointMass& pm = rbody.mPoints[i];

			////////////////////////////////////////////////
			// Draw Point Mass
			////////////////////////////////////////////////
			MatS.SetScale( 3.0f, 0.3f, 3.0f );
			MatS.SetTranslation( rbody.ComW() );
			targ->PushModColor( fvec3::Red() );
			targ->PushMMatrix( MatS );
			{
				matsolid.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
				lev2::CGfxPrimitives::GetRef().RenderDiamond( targ );

			}
			targ->PopModColor();
			targ->PopMMatrix();*/
		}
	};

	#if 0 //DRAWTHREADS
	ent::CallbackDrawable* pdrw = OrkNew ent::CallbackDrawable( pent );
	pent->AddDrawable( AddPooledLiteral("Default"), pdrw );
	pdrw->SetCallback( yo::doit );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetData( (const void*) pent );

	pent->GetDagNode().GetTransformNode().GetTransform()->SetMatrix( mRigidBody.mCurrentMatrix );
#endif

}

///////////////////////////////////////////////////////////////////////////////

void FighterControllerInst::CalcForces( float fddt )
{
	////////////////////////////////////
	int inump = mRigidBody.mPoints.size();
	const fmtx4& cmat = mRigidBody.mCurrentMatrix;
	const fvec3 CurrentPos = cmat.GetTranslation();
	const fvec3 CurrentVel = mRigidBody.mVelocity;
	const fvec3 World_COM = mRigidBody.ComW(); 
	
	////////////////////////////////
	// measure target motion
	////////////////////////////////
	
	//const fvec3 TargetPos = mTarget->GetDagNode().GetTransformNode().GetTransform()->GetPosition();
	//static fvec3 TargetLPos = TargetPos+fvec3(0.0f,1.0f,0.0f);
	//const fvec3 TargetVel = (TargetPos-TargetLPos)*(1.0f/fddt);
	//TargetLPos = TargetPos;

	////////////////////////////////
	////////////////////////////////
	// gravity
	////////////////////////////////

	fvec3 wgaccel( 0.0f, -90.8f, 0.0f ); // gravity
	fvec3 wgforce = wgaccel*mRigidBody.mTotalMass;

	mRigidBody.ApplyForce( wgforce, World_COM );

	////////////////////////////////
	// PID Controller Update
	////////////////////////////////

	// Delta to where we want to go

	const fvec3 DeltaPos = (mWaypoint-CurrentPos);

	float fdist = DeltaPos.Mag();

	if( fdist > 200.0f ) fdist = 200.0f;

	fvec3 IdealVel = DeltaPos.Normal()*fdist;
	
	fvec3 DeltaVel = (IdealVel-CurrentVel);
	fvec3 DeltaAcc = (DeltaVel*(0.4f/fddt)-wgaccel);

	//fvec3 Error = (IdealVel-CurrentVel);

	//float cor_X = mPositionController[0].Update(Error.GetX(),fddt);
	//float cor_Y = mPositionController[1].Update(Error.GetY(),fddt);
	//float cor_Z = mPositionController[2].Update(Error.GetZ(),fddt);
	//fvec3 rktforce( cor_X, cor_Y, cor_Z );

	// compute thruster from PID controller

	//if( mHitPoints>1.0f )
	{
		mRigidBody.ApplyForce( DeltaAcc*mRigidBody.mTotalMass, World_COM );
	}

	///////////////////////////////////////////////
	// friction 
	///////////////////////////////////////////////
	float fric = 0.001f;
	//fvec3 vel = mRigidBody.PointVelocityW(p);
	fvec3 acc = CurrentVel*(1.0f/fddt);
	fvec3 accxz = fvec3( acc.GetX(), acc.GetY(), acc.GetZ() )*fric;
	mRigidBody.ApplyForce( - accxz * mRigidBody.mTotalMass, World_COM );
	mRigidBody.mAngularMomentum *= 0.9999f;

	////////////////////////////////
	
	if( mThisTarget.mDamageImpulse.MagSquared() != 0.0f )
	{
		float fmag = mThisTarget.mDamageImpulse.Mag();
		fvec3 vimp = (mThisTarget.mDamageImpulse.Normal()+fvec3(0.0f,1.0f,0.0f))*0.5f;
		mRigidBody.ApplyImpulse( vimp*fmag, World_COM );
		mThisTarget.mDamageImpulse = fvec3(0.0f,0.0f,0.0f);
	}

}

///////////////////////////////////////////////////////////////////////////////

void FighterControllerInst::DoUpdate(ork::ent::SceneInst *sinst)
{
	float dt = sinst->GetDeltaTime();

	///////////////////////////////////////////
	// timer updates
	///////////////////////////////////////////

	mfRePosTimer -= dt;
	mfReAssTimer -= dt;
	mfMissileTimer -= dt;
	mLifeTime -= dt;

	///////////////////////////////////////////
	// Logic
	///////////////////////////////////////////

	fvec3 TargetPos = mTarget->GetDagNode().GetTransformNode().GetTransform().GetPosition();
	fvec3 MyPos = GetEntity()->GetDagNode().GetTransformNode().GetTransform().GetPosition();

	/////////////////////////////////
	// Reposition Timer
	/////////////////////////////////

	if( mfRePosTimer <= 0.0f )
	{
		mfRePosTimer = 0.25f;
		float fxd = ((float( rand()%32768 )/32768.0f) - 0.5f) * 5.0f;
		float fzd = ((float( rand()%32768 )/32768.0f) - 0.5f) * 5.0f;
		float fyd = ((float( rand()%32768 )/32768.0f) - 0.5f) * 5.0f;

		fvec3 ReqPos = mHotSpot->RequestPosition();
		fvec3 ToTgt = (TargetPos-ReqPos).Normal()*10.0f;

		fvec3 ToTgtCross = ToTgt.Cross(fvec3(0.0f,1.0f,0.0f));
		
		int irand = rand()%8;
		int irand2 = rand()%8;

		float frand = (float(irand)/8.0f)-0.5f;
		float frand2 = (float(irand2)/8.0f);

		fvec3 dir = ToTgt+(ToTgtCross*frand2);

		fvec3 HomeLoc = ReqPos + (dir*(frand*2.0f));
		fvec3 NextWp = HomeLoc + fvec3( fxd, 0.0f, fzd );
		fvec3 hfnextp, hfnextn;
		//mWCI->ReadSurface( NextWp, hfnextp, hfnextn );
		mWaypoint = hfnextp + fvec3(0.0f,8.0f+fyd,0.0f);
	}

	/////////////////////////////////
	// ReAssignment Timer
	/////////////////////////////////

	if( mfReAssTimer <= 0.0f )
	{
		mSpawner->ReAssignFighter( this );
		mfReAssTimer = 0.25f;
	}

	/////////////////////////////////
	// Missile Timer
	/////////////////////////////////

	float age = (kinitlife-mLifeTime);
	if( (mfMissileTimer <= 0.0f) && (age>5.0f) )
	{
		ShipControllerInst* sci = mSpawner->GetSCI();

		OrkAssert( sci );

		if( (MyPos-TargetPos).MagSquared() < (90.0f*90.0f) )
		{
			wiidom::LaunchMissile( sinst, GetEntity()->GetDagNode(), fvec3(0.0f,0.0f,0.0f), mWCI, sci->GetITarget(), 0.5f );
			mfMissileTimer = 1.5f;
		}
	}

	/////////////////////////////////

	switch( meState )
	{
		case ESTATE_RESET:
		case ESTATE_PLACEMENT:
		case ESTATE_DODGE:
		case ESTATE_ATTACK:
		{	break;
		}
	}

	/////////////////////////////////////////////
	// despawn control
	/////////////////////////////////////////////

	if( mHitPoints <= 0.0f )
	{
		DespawnSelf(sinst);
	}
	if( mLifeTime <= 0.0f )
	{
		DespawnSelf(sinst);
	}

	/////////////////////////////////////////////
	// Rigid Body Physics
	/////////////////////////////////////////////
	
	int itimediv = 1;
	for( int id=0; id<itimediv; id++ )
	{	float dt = sinst->GetDeltaTime();
		float fddt = dt/float(itimediv);
		//////////////////////
		// accum forces
		//////////////////////
		mRigidBody.BeginForces();
		{	
			CalcForces(fddt);
		}
		mRigidBody.EndForces();
		mRigidBody.IntegrateForces(fddt);
		//Collision(fddt);
		mRigidBody.IntegrateImpulses(fddt);
	}
	/////////////////////////////////////////////
	GetEntity()->GetDagNode().GetTransformNode().GetTransform().SetMatrix( mRigidBody.mCurrentMatrix );
	/////////////////////////////////////////////
}

void FighterControllerInst::DespawnSelf(ork::ent::SceneInst *sinst)
{
	sinst->QueueDeactivateEntity( GetEntity() );
	OrkAssert( mSpawner );
	mSpawner->Despawning( this );
	
	/*if( mHotSpot )
	{
		mHotSpot->RemoveFighter(this);
	}*/
}

void FighterControllerInst::SetHotSpot( HotSpot* hs )
{
	/*if( mHotSpot )
	{
		mHotSpot->RemoveFighter(this);
	}*/
	mHotSpot = hs;
	//mHotSpot->AddFighter(this);
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
