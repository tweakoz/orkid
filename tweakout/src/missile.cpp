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
#include <ork/asset/AssetManager.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include "missile.h"
#include "world.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::MissileControllerData, "MissileControllerData" );
//INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::MissileControllerInst, "MissileControllerInst" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void MissileControllerData::Describe()
{
	ork::ent::RegisterFamily<MissileControllerData>(ork::AddPooledLiteral("control"));
}

MissileControllerData::MissileControllerData()
{
	mpExplosionArchAsset = asset::AssetManager<ork::ent::ArchetypeAsset>::Load( "data://archetypes/particles/dust_cloud" );
}

ent::ComponentInst* MissileControllerData::createComponent(ent::Entity* pent) const
{
	return OrkNew MissileControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////
static lev2::XgmModelAsset* model = 0;

void LaunchMissile(	ent::SceneInst* sinst,
					const ent::DagNode& dn,
					const fvec3& InitialVelocity,
					WorldControllerInst* wci,
					ITarget& tgt,
					float fdmgmult )
{
	static MissileControllerData mcd;
	static const ent::EntData med;
	//static ork::ent::ModelComponentData mdlcd;

	if( 0 == model )
	{
		model = asset::AssetManager<lev2::XgmModelAsset>::Load( "data://actors/missile/missile" );
	}
	//mcd.SetModel( model );
	//mdlcd.SetScale( 1.0f );

	///////////////////////////////
	// create entity, init it's position to players position
	///////////////////////////////
	ent::Entity* pent = OrkNew ent::Entity( med, sinst );
	auto dnmtx = dn.GetTransformNode().GetTransform().GetMatrix();

	pent->GetDagNode().GetTransformNode().GetTransform().SetMatrix( dnmtx );

	///////////////////////////////
	// create entity components and connect them
	///////////////////////////////
	MissileControllerInst* mci = (MissileControllerInst*) mcd.createComponent(pent);
	mci->SetWCI( wci );
	mci->SetEntity( pent );
	mci->SetTarget( & tgt );
	mci->SetDamageMult( fdmgmult );
	mci->RigidBody().ApplyImpulse( InitialVelocity*mci->RigidBody().mTotalMass, mci->RigidBody().ComW() );

	///////////////////////////////

//	ork::ent::ModelComponentInst* mdlci = ork::rtti::autocast(mdlcd.createComponent(pent));

	///////////////////////////////
	// activate entity in sceneinst
	///////////////////////////////
	pent->GetComponents().AddComponent( mci );
	assert(false); // fixme
	//sinst->QueueActivateEntity( pent );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

//void MissileControllerInst::Describe()
//{
//}

MissileControllerInst::MissileControllerInst( const MissileControllerData& pcd, ork::ent::Entity* pent )
	: ent::ComponentInst( & pcd, pent )
	, mCD(pcd)
	, mLifeTime( 5.0f )
	, meState( ESTATE_RESET )
	, mTarget( 0 )
	, mDamageMult( 1.0f )
{
	////////////////////////////////////
	// missile's rigid body
	////////////////////////////////////

	ent::PointMass p0;
	const float pmass = 3000.0f;
	p0.mMass = pmass;
	p0.mMOI = ent::RigidBody::MassMOI_Sphere( 0.1f, p0.mMass );
	const float frad = 3.0f;
	const float cele = 0.0f;
	p0.mPosition.SetXYZ( -frad, cele, -frad );
	mRigidBody.AddPointMass( p0 );
	mRigidBody.mElasticity = 0.9f;
	mRigidBody.Close();

	////////////////////////////////////
	// PID controllers
	////////////////////////////////////
	
	static const float kprop = 0.9f;	// proportion per update to approach target
	static const float kintg = 0.0f;	// mystery value
	static const float kderi = 0.04f;	// minimum approach velocity

	static const fvec2 pidrange(-100.0f,100.0f);
	static const fvec2 pidmaxd(-30.0f,30.0f);
	mPIDController[0].Configure( kprop, kintg, kderi, pidrange, pidmaxd );
	mPIDController[1].Configure( kprop, kintg, kderi, pidrange, pidmaxd );
	mPIDController[2].Configure( kprop, kintg, kderi, pidrange, pidmaxd );



	mRigidBody.mPosition = pent->GetDagNode().GetTransformNode().GetTransform().GetPosition();
	
	mRigidBody.BeginForces();
	mRigidBody.EndForces();
	mRigidBody.IntegrateForces(0.0f);

	//////////////////////////////////////////////////////////////
	// init the entities drawable to this callback
	//////////////////////////////////////////////////////////////
	struct yo
	{

		static void doit( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren )
		{
	
			///////////////////////
			// Draw RigidBody Object
			///////////////////////
			auto user_data0 = pren->GetUserData0();
			const ent::Entity* pent = user_data0.Get<const ent::Entity*>();
			const MissileControllerInst* sci = pent->GetTypedComponent<MissileControllerInst>();
			OrkAssert( sci );
			MissileControllerInst* scinc = const_cast<MissileControllerInst*>(sci);
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
			fmtx4 MatE;
			fvec3 vdir = rbody.mVelocity.Normal();
			fvec3 vup = fvec3(1.0f,1.0f,1.0f);
			fvec3 vsid = vdir.Cross(vup);

			MatE.fromNormalVectors( vsid, vup, vdir );
			MatE.SetTranslation( rbody.mPosition );
			fmtx4 MatS; MatS.SetScale( 0.3f, 0.3f, 3.0f );
			targ->PushModColor( fvec3::White() );
			targ->MTXI()->PushMMatrix( MatS*MatE );

			ork::lev2::RenderContextInstModelData rcimd;
			ork::lev2::XgmModelInst modelinst( model->GetModel() );
			rcimd.mpModelInst = & modelinst;
			rcimd.mMesh = model->GetModel()->GetMesh(0);
			rcimd.mSubMesh = rcimd.mMesh->GetSubMesh(0);
			rcimd.mCluster = & rcimd.mSubMesh->RefCluster(0);
			rcimd.miSubMeshIndex = 0;
			{
				matsolid.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR );
				lev2::CGfxPrimitives::GetRef().RenderDiamond( targ );

				//model->RenderRigid(vup,MatE,targ,rcid,rcimd);

			}
			targ->PopModColor();
			targ->MTXI()->PopMMatrix();
		}
	};

	#if 0 //DRAWTHREADS
	ent::CallbackDrawable* pdrw = OrkNew ent::CallbackDrawable( pent );
	pent->AddDrawable( AddPooledLiteral("Default"),pdrw );
	pdrw->SetCallback( yo::doit );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetData( (const void*) pent );
	#endif
}

///////////////////////////////////////////////////////////////////////////////

void MissileControllerInst::Detonate(ork::ent::SceneInst *sinst, const ork::fmtx4& mtx)
{
#if 0

	static ork::ent::EntData myentdata;
	ork::ent::Entity* pent = OrkNew ork::ent::Entity(myentdata,sinst);
	ork::ent::Archetype* parch = mCD.GetExplosionArchetype()->GetArchetype();

	parch->ComposeEntity( pent );
	
	fmtx4 mtxs;
	mtxs.SetScale(0.1f);
	fmtx4 mtx2 = mtx*mtxs;

	pent->GetDagNode().GetTransformNode().GetTransform()->SetMatrix( mtx2 );

	sinst->QueueActivateEntity( pent );

#endif
}

///////////////////////////////////////////////////////////////////////////////

void MissileControllerInst::DoUpdate(ork::ent::SceneInst *sinst)
{
	float dt = sinst->GetDeltaTime();

	///////////////////////////////////////////

	const fmtx4& cmat = mRigidBody.mCurrentMatrix;
	const fvec3 CurrentPos = cmat.GetTranslation();
	const fvec3 DeltaPos = (mWaypoint-CurrentPos);

	///////////////////////////////////////////
	// Logic
	///////////////////////////////////////////

	fvec3 TargetPos = mTarget->GetPos();

	fvec3 TargetVel = (TargetPos-mLastTargetPos)*(1.0f/dt);

	mLastTargetPos = TargetPos;

	switch( meState )
	{
		case ESTATE_RESET:
		{	OrkAssert( mTarget );
			if( mftimer <= 0.0f )
			{
				//mEntity->GetDagNode().GetTransformNode().GetTransform()->SetPosition( mHomeLoc + fvec3( 0.0f, 50.0f, 0.0f ) );
				meState = ESTATE_AQUIRE;
				mWaypoint = TargetPos+(TargetVel*0.5f);
			}
			break;
		}
		case ESTATE_AQUIRE:
		{	
			if( mftimer <= 0.0f )
			{
				mftimer = 0.1f;
				mWaypoint = TargetPos+(TargetVel*0.2f)+fvec3(0.0f,0.0f,0.0f);
			}
			if( mLifeTime <= 0.0f )
			{
				sinst->QueueDeactivateEntity( GetEntity() );
			}
			if( DeltaPos.Mag() < 5.0f )
			{
				meState = ESTATE_DETONATE;
			}
			
			////////////////////////////////////
			// if we hit the ground, detonate
			////////////////////////////////////

			fvec3 hfnextp, hfnextn;
			//mWCI->ReadSurface( CurrentPos, hfnextp, hfnextn );

			if( CurrentPos.GetY() < hfnextp.GetY() )
			{
				meState = ESTATE_DETONATE;
			}

			break;
		}
		case ESTATE_DETONATE:
		{

			if( DeltaPos.Mag() < 5.0f )
			{
				mTarget->NotifyDamage( mRigidBody.mVelocity*mRigidBody.mTotalMass*mDamageMult );
			}

			fmtx4 mtx;
			mtx.SetTranslation( CurrentPos );
			Detonate(sinst,mtx);
			
			sinst->QueueDeactivateEntity( GetEntity() );

			break;		
		}
		case ESTATE_SEEK:
		{	break;
		}
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
	
	////////////////////////////////////////////////
	// Draw Point Mass
	////////////////////////////////////////////////
	fmtx4 MatE;
	fvec3 vdir = mRigidBody.mVelocity.Normal();
	fvec3 vup = fvec3(1.0f,1.0f,1.0f);
	fvec3 vsid = vdir.Cross(vup);

	MatE.fromNormalVectors( vsid, vup, vdir );
	MatE.SetTranslation( mRigidBody.mPosition );

	////////////////////////////////////////////////

	mftimer -= dt;
	mLifeTime -= dt;
}

///////////////////////////////////////////////////////////////////////////////

void MissileControllerInst::CalcForces( float fddt )
{
	////////////////////////////////////
	int inump = mRigidBody.mPoints.size();
	const fmtx4& cmat = mRigidBody.mCurrentMatrix;
	const fvec3 CurrentPos = cmat.GetTranslation();
	const fvec3 CurrentVel = mRigidBody.mVelocity;
	const fvec3 World_COM = mRigidBody.ComW(); 
	
	////////////////////////////////
	// gravity
	////////////////////////////////

	fvec3 wgaccel( 0.0f, -9.8f, 0.0f ); // gravity
	fvec3 wgforce = wgaccel*mRigidBody.mTotalMass;

	mRigidBody.ApplyForce( wgforce, World_COM );

	////////////////////////////////
	// PID Controller Update
	////////////////////////////////

	const fvec3 DeltaPos = (mWaypoint-CurrentPos);

	fvec3 IdealVel = DeltaPos.Normal()*150.0f;
	
	fvec3 DeltaVel = (IdealVel-CurrentVel);
	fvec3 DeltaAcc = (DeltaVel*(0.3f/fddt)-wgaccel);

	float da_mag = DeltaAcc.Mag();

	if( da_mag > 150.0f )
	{
		DeltaAcc = DeltaAcc.Normal()*150.0f;
	}

	mRigidBody.ApplyForce( DeltaAcc*mRigidBody.mTotalMass, World_COM );

	///////////////////////////////////////////////
	// friction 
	///////////////////////////////////////////////
	float fric = 0.000001f;
	fvec3 acc = CurrentVel*(1.0f/fddt);
	fvec3 accxz = fvec3( acc.GetX(), acc.GetY(), acc.GetZ() )*fric;
	mRigidBody.ApplyForce( - accxz * mRigidBody.mTotalMass, World_COM );
	mRigidBody.mAngularMomentum *= 0.9999f;


	////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
