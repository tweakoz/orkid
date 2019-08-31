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
#include <pkg/ent/ModelComponent.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/box.h>
#include <ork/gfx/camera.h>
///////////////////////////////////////////////////////////////////////////////
#if 0
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include "ship.h"
#include "world.h"
#include "missile.h"
#include "enemy_fighter.h"
#include "enemy_spawner.h"
#include <pkg/ent/bullet.h>
#include <ork/math/basicfilters.h>
//
#include <BulletCollision/CollisionShapes/btMultiSphereShape.h>
#include <pkg/ent/ParticleControllable.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::ShipArchetype, "Tweakout/ShipArchetype" );
INSTANTIATE_TRANSPARENT_RTTI( ork::wiidom::ShipControllerData, "Tweakout/ShipControllerData" );
///////////////////////////////////////////////////////////////////////////////
using namespace ork::ent;

namespace ork {
namespace ent { void RigidBody_Draw( lev2::GfxTarget* targ, const CMatrix4& matw, const RigidBody& rbody, bool bdebug ); }
namespace wiidom {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static const int kcenterpoint = 8;

///////////////////////////////////////////////////////////////////////////////

bool ShipControllerInst::DoLink(ent::SceneInst* psi)
{
	this->SetWCI( psi->FindTypedEntityComponent<WorldControllerInst>("/ent/world") );

	auto this_ent = GetEntity();


	if( auto bulletsys = psi->findSystem<ork::ent::BulletSystem>() ){

			const auto& world_data = bulletsys->GetWorldData();
			btVector3 grav = !world_data.GetGravity();

			if(btDynamicsWorld *world = bulletsys->GetDynamicsWorld())
			{
				const float pmass = 20000.0f;
				const float prad = 3.0f;
				const int knumspheres = 10;

				auto sphere_poss = new btVector3[knumspheres];
				auto sphere_rads = new btScalar[knumspheres];

				for( int i=0; i<knumspheres; i++ )
					sphere_rads[i] = prad;

				const float frad = 18.0f;
				const float cele = 0.0f;
				sphere_poss[0] = btVector3( -frad, cele, -frad );
				sphere_poss[1] = btVector3( +frad, cele, -frad );
				sphere_poss[2] = btVector3( -frad, cele, +frad );
				sphere_poss[3] = btVector3( +frad, cele, +frad );
				sphere_poss[4] = btVector3( +frad*1.41f, cele, 0.0f );
				sphere_poss[5] = btVector3( -frad*1.41f, cele, 0.0f );
				sphere_poss[6] = btVector3( 0.0f, cele, +frad*1.41f );
				sphere_poss[7] = btVector3( 0.0f, cele, -frad*1.41f );
				sphere_poss[8] = btVector3( 0.0f, -0.3f, 0.0f );
				sphere_poss[9] = btVector3( 0.0f, 9.0f, 0.0f );

				#if 0
				p0.mMass = pmass;	p1.mMass = pmass;	p2.mMass = pmass;	p3.mMass = pmass;
				p4.mMass = pmass;	p5.mMass = pmass;	p6.mMass = pmass;	p7.mMass = pmass;
				p8.mMass = pmass*9.0f;
				p9.mMass = pmass*0.1;
				//mRigidBody.AddPointMass( p9 );
				mRigidBody.mPosition = CVector3( 0.0f, 200.0f, 0.0f );
				mRigidBody.mElasticity = 0.90f;
				//////////////////////////////////////
				p0.mPosition.SetXYZ( 1.0f,0.0f,0.0f );
				p0.mMass = 1e6f;
				mGroundBody.AddPointMass( p0 );
				p0.mPosition.SetXYZ( -1.0f,0.0f,0.0f );
				mGroundBody.AddPointMass( p0 );
				p0.mPosition.SetXYZ( 0.0f,0.0f,1.0f );
				mGroundBody.AddPointMass( p0 );
				mGroundBody.Close();
				mGroundBody.mPosition = CVector3( 0.0f, 0.0f, 0.0f );
				mGroundBody.BeginForces();
				mGroundBody.EndForces();
				mGroundBody.IntegrateForces(0.0f);
				//////////////////////////////////////
				#endif

				DagNode& dnode = this_ent->GetDagNode();
				TransformNode& t3d = dnode.GetTransformNode();
				CMatrix4 mtx_e = t3d.GetTransform().GetMatrix();
				CMatrix4 mtx_r; //mtx_r.SetRotateX(PI*0.5f);
				auto mtx = (mtx_r*mtx_e);

				auto ship_shape = new btCylinderShape (btVector3(frad,prad,frad));
				float total_mass = pmass*float(knumspheres);

				mRigidBody = bulletsys->AddLocalRigidBody(this_ent,10.0f,!mtx,ship_shape);
				mRigidBody->setGravity(grav);
				mRigidBody->setActivationState(WANTS_DEACTIVATION);
				mRigidBody->activate();

				mRigidBody->setRestitution(0.5f);
				mRigidBody->setFriction(0.5f);
				mRigidBody->setDamping( 0.5f,0.5f );

		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

ShipControllerInst::ShipControllerInst( const ShipControllerData& pcd, ent::Entity *entity )
	: ent::ComponentInst( & pcd, entity )
	, mPcd(pcd)
	, mWCI( 0 )
	, mThisTarget(*this)
	, mRigidBody(nullptr)
{

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
			const Entity* pent = user_data0.Get<const Entity*>();
			const ShipControllerInst* sci = pent->GetTypedComponent<ShipControllerInst>();

			if( 0 == sci )
			{
				sci = pent->GetTypedComponent<ShipControllerInst>();
			}
			OrkAssert( sci );
			ShipControllerInst* scinc = const_cast<ShipControllerInst*>(sci);
			CMatrix4 matw;
			pent->GetDagNode().GetTransformNode().GetMatrix( matw );
			//RigidBody_Draw( targ, matw, scinc->RigidBody(), scinc->GetSCD().GetDebug() );
		}
	};

	#if 0 //DRAWTHREADS
	auto pdrw = OrkNew CallbackDrawable( entity );
	entity->AddDrawable( AddPooledLiteral("Default"),pdrw );
	pdrw->SetCallback( yo::doit );
	pdrw->SetOwner(  & entity->GetEntData() );
	pdrw->SetData( (const ent::Entity*) entity );
	#endif
}

///////////////////////////////////////////////////////////////////////////////

CVector3 ShipTarget::GetPos()
{
	return CVector3(); //mSCI.RigidBody().mPosition;
}
void ShipTarget::NotifyDamage(const CVector3& Impulse)
{
	mDamageImpulse += Impulse;
}

///////////////////////////////////////////////////////////////////////////////

bool ShipControllerInst::Collision(float fddt)
{
	bool brval = false;

	#if 0
	int inump = mRigidBody.mPoints.size();

	if( mThisTarget.mDamageImpulse.MagSquared() != 0.0f )
	{
		mRigidBody.ApplyImpulse( mThisTarget.mDamageImpulse, mRigidBody.ComW() );
		mThisTarget.mDamageImpulse = CVector3(0.0f,0.0f,0.0f);
	}

	for( int ip=0; ip<inump; ip++ )
	{	const CVector3 wp  = mRigidBody.PntW(ip);
		const CVector3 NextPos = wp+mRigidBody.PointVelocityW( wp )*fddt;
		mRigidBody.mPoints[ip].mImpulse = CVector3(0.0f,0.0f,0.0f);
		/////////////////////////////////
		/////////////////////////////////
		CVector3 GroundCollideNormal,CollisionPoint;
		float fat;
		CVector3 curhfxyz, curhfn;
		CVector3 tp = (wp+NextPos)*0.5f;
		mWCI->ReadSurface( tp, curhfxyz, curhfn );
		float fh = (tp.GetY()-curhfxyz.GetY());
		mRigidBody.mPoints[ip].mCollisionDepth = 0.0f;
		mRigidBody.mPoints[ip].mLastPointHeight = mRigidBody.mPoints[ip].mPointHeight;
		mRigidBody.mPoints[ip].mPointHeight = fh;
		mRigidBody.mPoints[ip].mColNrm = CVector3( 0.0f,1.0f,0.0f);
		mRigidBody.mPoints[ip].mImpulse = CVector3( 0.0f,0.0f,0.0f);
		mRigidBody.mPoints[ip].msimp = 0.0f;

		if( mWCI->CollisionCheck( wp, NextPos, CollisionPoint, GroundCollideNormal, fat ) )
		/////////////////////////////////
		{	// collision response
			//////////////////////////////////////////
			// calulate impulse
			//////////////////////////////////////////
			float impulse = ent::RigidBody::SingleBodyImpulse(
					GroundCollideNormal,
					CollisionPoint,
					mRigidBody,
					mRigidBody.mPoints[ip].mMass ); // * (1.0f-fat); // * fddt;
			/////////////////////////////////
			CVector3 MagImp = GroundCollideNormal*impulse; //*(1.0f-fat);
			/////////////////////////////////
			if( impulse>0.0f ) // && (impulse<10.0f) )
			{	mRigidBody.ApplyImpulse( MagImp, CollisionPoint );
				float fl = logf( impulse );
				//if( fl > 0.5f )
				//{	irumbctr += 3;
				//}
			}
			mRigidBody.mPoints[ip].mImpulse = MagImp;
			/////////////////////////////////
			mRigidBody.mPoints[ip].mCollisionDepth = (CollisionPoint-NextPos).Mag();
			mRigidBody.mPoints[ip].mColNrm = GroundCollideNormal;
			mRigidBody.mPoints[ip].msimp = impulse;
			brval |= true;
		}
		mRigidBody.mPoints[ip].mColPos = CollisionPoint;
	}
#endif
	return brval;
}

///////////////////////////////////////////////////////////////////////////////

bool ShipControllerInst::DoUpdate_Flip( const ork::lev2::InputState& inpstate, float fdt )
{
	bool bv = false;

	static float fdta = 0.0f;

	ork::CQuaternion ident = ork::CQuaternion();
	if( inpstate.IsDown( lev2::ETRIG_RAW_JOY0_RDIG_UP ) )
	{
#if 0
		mRigidBody.mOrientation = ork::CQuaternion::Lerp( mRigidBody.mOrientation, ident, fdt*2.0f );
		mRigidBody.mPosition += ork::CVector3(0.0f,fdt*3.0f,0.0f);
		mRigidBody.mVelocity = 0.0f;
		mRigidBody.mLinAccel *= 0.0f;
		mRigidBody.mAngularVelocity = 0.0f;
		mRigidBody.mAngAccel = 0.0f;
		mRigidBody.mLinearMomentum = CVector3();
		mRigidBody.mAngularMomentum = CVector3();
#endif
		bv = true;
	}

	return bv;
}

///////////////////////////////////////////////////////////////////////////////

void ShipControllerInst::DoUpdate(SceneInst* sinst)
{
    /*
	float fddt = sinst->GetDeltaTime();

	DagNode* shipdn = & GetEntity()->GetDagNode();

	ork::lev2::CInputDevice *inputdevice = ork::lev2::CInputManager::GetRef().GetInputDevice(0);
	//inputdevice->Input_Poll(0);

	if( false == inputdevice->IsActive() )
	{
		inputdevice->Activate();
	}

	const ork::lev2::InputState DefaultState;

	const ork::lev2::InputState& inpstate = inputdevice ? inputdevice->RefInputState() : DefaultState;

	///////////////////////////////////////////////////////
	// set model orientation
	///////////////////////////////////////////////////////

	ork::ent::ModelComponentInst* mci = GetEntity()->GetTypedComponent<ork::ent::ModelComponentInst>();

	if( mci )
	{
		const CCameraData* cdata = sinst->GetCameraData( ork::AddPooledLiteral("game") );

		if( cdata )
		{
			const ork::CVector3& vz = cdata->GetZNormal();

			float fat2 = atan2( vz.GetZ(), vz.GetX() );

			const ork::ent::ModelComponentData& mdata = mci->GetData();
			ork::ent::ModelComponentData& ncdata = const_cast<ork::ent::ModelComponentData&>( mdata );
			ork::CVector3 rotate = ncdata.GetRotate();
			rotate.SetZ( fat2 );

			ncdata.SetRotate(rotate);
		}

	}

	////////////////////////////////////
	// get input
	////////////////////////////////////

	bool bspacetriggeredgedown = inpstate.IsDownEdge( ' ' );
	bool bspacetriggeredgeup = inpstate.IsUpEdge( ' ' );
	bool bwiiAedgedown = inpstate.IsDownEdge( lev2::ETRIG_RAW_JOY0_RDIG_DOWN );
	bool bwiiAedgeup = inpstate.IsUpEdge( lev2::ETRIG_RAW_JOY0_RDIG_DOWN );
	float fwiimotionx = inpstate.GetPressure( lev2::ETRIG_RAW_JOY0_MOTION_X );
	float fwiimotiony = inpstate.GetPressure( lev2::ETRIG_RAW_JOY0_MOTION_Y );
	float fwiimotionz = inpstate.GetPressure( lev2::ETRIG_RAW_JOY0_MOTION_Z );
	static avg_filter<16> filtx;
	static avg_filter<16> filty;
	static avg_filter<16> filtz;
	filtx.mbEnable = true;
	filty.mbEnable = true;
	filtz.mbEnable = true;
	float fx = filtx.compute( fwiimotionx );
	float fy = filty.compute( fwiimotiony );
	float fz = filtz.compute( fwiimotionz );
	const CVector3 wiimotion = -CVector3( fx, fy, fz ).Normal();
	static int irumbctr = 0;
	/////////////////////
	float lanay = inpstate.GetPressure(lev2::ETRIG_RAW_JOY0_LANA_YAXIS);
	float ranay = inpstate.GetPressure(lev2::ETRIG_RAW_JOY0_RANA_YAXIS);
	/////////////////////
	if( bwiiAedgedown )
	{
		LaunchMissile(sinst);
	}

	//////////////////////////////////////////////////////////
	// physics
	//////////////////////////////////////////////////////////

	if( nullptr == mRigidBody )
		return;

	CMatrix4 mtx_ent;

	//////////////////////////////////////////////////
	// copy motion state to entity transform
	//////////////////////////////////////////////////

	const btMotionState* motionState = mRigidBody->getMotionState();
	btTransform xf;
	motionState->getWorldTransform(xf);
	mtx_ent = ! xf;
	GetEntity()->SetDynMatrix(mtx_ent);

	//////////////////////////////////////////////////////////

	auto ship_center = mtx_ent.GetTranslation();
	auto ship_xnorm = mtx_ent.GetXNormal();
	auto ship_ynorm = mtx_ent.GetYNormal();
	auto ship_znorm = mtx_ent.GetZNormal();

		//printf( "ship<%f %f %f\n",
		//	ship_center.GetX(), ship_center.GetY(), ship_center.GetZ() );

	//////////////////////////////////////////////////////////

	if( lanay>0.01f )
	{	//mRigidBody->applyCentralForce( !(ship_ynorm*lanay*1e2) );
		auto f = CVector3(0.0f,lanay*3e3,0.0f);

		//printf( "force<%f %f %f\n",
		//	f.GetX(), f.GetY(), f.GetZ() );

		mRigidBody->applyCentralForce( ! f );
	}

	if( ranay>0.01f )
	{	//mRigidBody->applyCentralForce( !(ship_ynorm*lanay*1e2) );
		auto f = ship_ynorm*ranay*-2.4e5;

		//printf( "force<%f %f %f\n",
		//	f.GetX(), f.GetY(), f.GetZ() );

		mRigidBody->applyTorque( ! f );
	}

	//////////////////////////////////////////////////////////
	// lift due to spin
	//////////////////////////////////////////////////////////

	CVector3 lift = (!mRigidBody->getAngularVelocity())*1.5e1;

	if( lift.Dot(CVector3(0.0f,1.0f,0.0f)) < 0.0f ) lift = lift*-1.0f;

	//mRigidBody->applyCentralForce( !lift );

	//////////////////////////////////////////////////////////
	// vertical stabilizer
	//////////////////////////////////////////////////////////

	CVector3 stab_axis = (CVector3(0.0f,1.0f,0.0f).Cross(ship_ynorm)).Normal();
	float stab_amt = 1.0f-(CVector3(0.0f,1.0f,0.0f).Dot(ship_ynorm));

	/*printf( "stab<%f %f %f> amt<%f>\n",
			stab_axis.GetX(), stab_axis.GetY(), stab_axis.GetZ(), stab_amt );

	if( ship_xnorm.Dot(CVector3(1.0f,0.0f,0.0f)) > 0.5f )
		mRigidBody->applyTorque( !(stab_axis*stab_amt*-3e4) );
	#endif

	//////////////////////////////////////////////////////////


	////////////////////////////////////
	// Rigid Body Dynamics
	////////////////////////////////////
	int inump = 10; //mRigidBody.mPoints.size();
	float point_mass = 20000.0f;
	float total_mass = point_mass*float(inump);
	//const CMatrix4& cmat = mRigidBody.mCurrentMatrix;
	//const CVector3 CurrentPos = cmat.GetTranslation();
	//const CVector3 wctr = mRigidBody.ComW();
	//////////////////////
	// accum forces
	//////////////////////
	//mRigidBody.BeginForces();
	{
		////////////////////////////////////////////////
		////////////////////////////////////////////////
		////////////////////////////////////////////////
		bool bflip = false; //DoUpdate_Flip( inpstate, fddt );
		////////////////////////////////////////////////
		////////////////////////////////////////////////
		////////////////////////////////////////////////


		if( false == bflip )
		{
			///////////////////////////////////////////////
			// per point
			///////////////////////////////////////////////

			//CVector3 ctrW = mRigidBody.PntW(kcenterpoint);
			for( int ip=0; ip<inump; ip++ )
			{
				float fphi = PI2*float(ip)/float(inump);
				float fphi_sin = sinf(fphi);
				float fphi_cos = cosf(fphi);

				CVector3 point_pos = ship_center+(ship_xnorm*fphi_sin)+(ship_znorm*fphi_cos);
				CVector3 vup = ship_ynorm;

				////////////////////////////////////////////////
				// ship vertical thrusters
				////////////////////////////////////////////////

				//printf( "lanay<%f>\n", lanay );
				if( lanay>0.01f )
				{
					//if( ip != kcenterpoint ) // spinner
					{
						CVector3 center_to_edge_dir = (point_pos-ship_center).Normal();
						CVector3 vsp = ship_ynorm.Cross(center_to_edge_dir);
						CVector3 tf = ship_ynorm*point_mass*5000.0f*(lanay);

						printf( "apply force<%f %f %f> pos<%f %f %f> p<%d>\n",
								tf.GetX(), tf.GetY(), tf.GetZ(),
								point_pos.GetX(), point_pos.GetY(), point_pos.GetZ(),
								ip );

						//mRigidBody->applyForce( !tf, !point_pos );
					}
				}
				if( ip == kcenterpoint ) // Lift Due to Spin
				{
					//CVector3 lift = mRigidBody.mAngularVelocity*tmass*5.0f;

					//if( lift.Dot(CVector3(0.0f,1.0f,0.0f)) < 0.0f ) lift = lift*-1.0f;

					//mRigidBody.ApplyForce( lift, p );
				}

				#if 0
				////////////////////////////////////////////////
				// ship lateral thrusters
				////////////////////////////////////////////////
				if( ranay>0.01f )
				{
					const CMatrix4& mz = cmat;
					CVector3 vdir, vup, vsid;
					////////////////////////////////////////////
					static CVector3 vdirac = CVector3(0.0f,0.0f,-15.0f);
					vdir = (mRigidBody.mVelocity*CVector3(1.0f,0.0f,1.0f)).Normal();
					vdirac += vdir;
					vdirac = vdirac.Normal()*15.0f;
					if( vdirac.Mag() == 0.0f )
					{
						orkprintf( "vdirac.Mag==0.0f\n" );
						vdirac = CVector3(0.0f,0.0f,-15.0f);
					}

					vdir = vdirac.Normal();
					vup = CVector3(0.0f,1.0f,0.0f);
					vsid = vdir.Cross( vup );
					CMatrix4 MatShipRot;
					MatShipRot.NormalVectorsIn( vsid, vup, vdir );
					////////////////////////////////////////////
					float fturn = (fx*mPcd.GetSteeringAngle());
					float ftm = fabs(fx);
					//float fforcebas = 20.0f;
					////////////////////////////////////////////
					CQuaternion q;
					q.FromAxisAngle( CVector4( 0.0f, 1.0f, 0.0f, fturn ) );
					MatShipRot = q.ToMatrix()*MatShipRot;
					CVector3 fdir	=	CVector3(0.0f,0.0f,mPcd.GetForwardForce()
									+	(ftm*mPcd.GetForwardForce()*mPcd.GetSteeringRatio()))
									.	Transform3x3(MatShipRot);

					if( ip==kcenterpoint )
					{
						const float khmax = 5.0f;
						if( fheight > khmax )
						{
							CVector3 downforce( 0.0f, -1.0f, 0.0f );
							mRigidBody.ApplyForce( downforce*tmass, p );
						}
						else
						{
							float fconst = ranay*3.0f; //(khmax-fheight)/khmax;
							mRigidBody.ApplyForce( fdir*tmass*fconst, p );
						}
					}
					////////////////////////////////////////////
				}
				///////////////////////////////////////////////
				// friction
				///////////////////////////////////////////////
				float fric = bcol ? mPcd.GetGroundFriction() : mPcd.GetAirFriction();
				CVector3 vel = mRigidBody.PointVelocityW(p);
				CVector3 acc = vel*(1.0f/fddt);
				CVector3 accxz = CVector3( acc.GetX(), 0.0f, acc.GetZ() )*fric;
				if( ip == kcenterpoint )
				{
					mRigidBody.ApplyForce( - accxz * tmass, p );
				}
				mRigidBody.mAngularMomentum *= 0.9999f;
				///////////////////////////////////////////////
				// vertical stabilizer and lift
				///////////////////////////////////////////////
				if( 1 ) // LIFT due to forward motion
				{
					const float kh = 3.0f;
					const float kp = 1.3f;
					float fmag = powf(kh-fheight,1.0f/kp) * mRigidBody.mVelocity.Mag()/20.0f;
					if( fmag>0.0f )
					{
						CVector3 tf = -(cnorm*pmass*9.8f) * fmag;
						//mRigidBody.ApplyForce( tf*(1.0003f/powf(kh,1.0f/kp)), p );
					}
				}
				if( 0 ) // ip != kcenterpoint ) // STABILIZER
				{
					int inext = (ip+1)%kcenterpoint;

					CVector3 del = (mRigidBody.PntW(ip)-ctrW).Normal();
					CVector3 delnex = (mRigidBody.PntW(inext)-ctrW).Normal();

					CVector3 vup = mRigidBody.mCurrentMatrix.GetYNormal(); //  del.Cross(delnex).Normal();

					float dh = (p.GetY()-ctrW.GetY());
					//float fmag = (1.0f-fheight);
					CVector3 tf = vup*pmass*1.0f*dh;

					if( tf.Mag() < 10.0f )
					{
						mRigidBody.ApplyForce( tf, p );
					}
				}
				#endif
			}
		}
	}
	///////////////////////////////////////////////
	//mRigidBody.EndForces();
	//mRigidBody.IntegrateForces(fddt);
	//Collision(fddt);
	//mRigidBody.IntegrateImpulses(fddt);
	///////////////////////////////////////////////

	//shipdn->GetTransformNode().GetTransform()->SetMatrix( mRigidBody.mCurrentMatrix );



	///////////////////////////////////////////////////////
*/
}

///////////////////////////////////////////////////////////////////////////////

void ShipControllerInst::LaunchMissile(ent::SceneInst* sinst)
{
	#if 0
	EnemySpawnerControllerInst* spwci = sinst->FindTypedEntityComponent<EnemySpawnerControllerInst>("/ent/spawner");

	const CVector3& MyPos = mRigidBody.mPosition;
	const CVector3& MyVel = mRigidBody.mVelocity;
	const CVector3& MyDir = MyVel.Normal();

	if( spwci )
	{
		const orkvector<FighterControllerInst*>& Fighters = spwci->Fighters();
		int inumf = int(Fighters.size());

		FighterControllerInst* tfsi = 0;

		float fmind = CFloat::TypeMax();

		CVector3 testpoint;
		CVector3 testpointHF, testpointHFN;

		static const int kmaxc = 4;
		static FighterControllerInst* Candidates[kmaxc];
		int NumCandidates = 0;

		for( int i=0; i<inumf; i++ )
		{
			FighterControllerInst* fsi = Fighters[i];

			const CVector3& fsipos = fsi->RigidBody().mPosition;

			//////////////////////////////////////////
			// first is it in range ?
			//////////////////////////////////////////

			float fsidist = (fsipos-MyPos).Mag();

			if( fsidist < 175.0f )
			{
				//////////////////////////////////////////
				// is it in front of us ?
				//////////////////////////////////////////

				CVector3 DirToEnem = (fsipos-MyPos).Normal();

				if( MyDir.Dot( DirToEnem ) > 0.3f )
				{
					//////////////////////////////////////////
					// is it obscured by terrain ?
					//////////////////////////////////////////

					bool bobsc = false;

					for( float fi=0.0f; fi<=1.0f; fi+=0.05f )
					{
						testpoint.Lerp( MyPos, fsipos, fi );
						mWCI->ReadSurface( testpoint, testpointHF, testpointHFN );

						if( testpoint.GetY() < testpointHF.GetY() )
						{
							bobsc = true;
							break;
						}

					}


					if( false == bobsc )
					{
						CVector3 proj = MyPos + (MyDir*fsidist);
						float distfr = (fsipos-proj).MagSquared();

						if( distfr < fmind )
						{
							fmind = distfr;
							tfsi = fsi;
						}

						if( NumCandidates < kmaxc )
						{
							Candidates[NumCandidates] = tfsi;
							NumCandidates++;
						}

					}
				}
			}
		}

		if( NumCandidates )
		{
			int irand = rand()%NumCandidates;
			tfsi = Candidates[irand];

			float fmag = mRigidBody.mVelocity.Mag();
			CVector3 mdir = (mRigidBody.mVelocity+CVector3(0.0f,0.5f,0.0f)).Normal();

			wiidom::LaunchMissile( sinst, GetEntity()->GetDagNode(), mdir*fmag, mWCI, tfsi->GetITarget(), 3.0f );
		}
	}
	#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ShipArchetype::Describe()
{
	//reflect::AnnotateClassForEditor<WorldArchetype>( "editor.instantiable", "false" );
}

///////////////////////////////////////////////////////////////////////////////

ShipArchetype::ShipArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void ShipArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ShipControllerData>();
	composer.Register<ent::ModelComponentData>();
	composer.Register<ork::psys::ParticleControllableData>();
	//sscomposer.Register<ent::BulletObjectControllerData>();
}

void ShipArchetype::DoStartEntity(ork::ent::SceneInst*, const ork::CMatrix4& mtx, ork::ent::Entity* pent ) const
{
	//pent->GetDagNode().GetTransformNode().GetTransform()->SetMatrix(mtx);

	psys::ParticleControllableInst* pci = pent->GetTypedComponent<psys::ParticleControllableInst>();
	if(pci)
		pci->Reset();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ShipControllerData::Describe()
{
	ork::ent::RegisterFamily<ShipControllerData>(ork::AddPooledLiteral("bullet"));

	reflect::RegisterProperty( "DT", & ShipControllerData::mDT );

	reflect::RegisterProperty( "ForwardForce", & ShipControllerData::mForwardForce );
	reflect::RegisterProperty( "SteeringPower", & ShipControllerData::mSteeringRatio );
	reflect::RegisterProperty( "SteeringAngle", & ShipControllerData::mSteeringAngle );
	reflect::RegisterProperty( "Gravity", & ShipControllerData::mGravity );

	reflect::RegisterProperty( "GroundFriction", & ShipControllerData::mGroundFriction );
	reflect::RegisterProperty( "AirFriction", & ShipControllerData::mAirFriction );

	reflect::RegisterProperty( "Debug", & ShipControllerData::mDebug );

	reflect::RegisterProperty( "FlipForce", & ShipControllerData::mfFlipForce );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "DT", "editor.range.min", "0.00001" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "DT", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "DT", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "GroundFriction", "editor.range.min", "0.000001" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "GroundFriction", "editor.range.max", "0.01" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "GroundFriction", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "AirFriction", "editor.range.min", "0.000001" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "AirFriction", "editor.range.max", "0.01" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "AirFriction", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "ForwardForce", "editor.range.min", "0.1" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "ForwardForce", "editor.range.max", "100.0" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "ForwardForce", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "SteeringPower", "editor.range.min", "0.01" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "SteeringPower", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "SteeringPower", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "SteeringAngle", "editor.range.min", "0.1" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "SteeringAngle", "editor.range.max", "6.28" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "Gravity", "editor.range.min", "1.0" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "Gravity", "editor.range.max", "50.0" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "Gravity", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "FlipForce", "editor.range.min", "500.0" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "FlipForce", "editor.range.max", "10000.0" );

	reflect::AnnotatePropertyForEditor<ShipControllerData>( "Debug", "editor.range.min", "0" );
	reflect::AnnotatePropertyForEditor<ShipControllerData>( "Debug", "editor.range.max", "1" );
}

///////////////////////////////////////////////////////////////////////////////

ShipControllerData::ShipControllerData()
	: mDT( 4.0f )
	, mForwardForce(20.0f)
	, mSteeringRatio(0.35f)
	, mGravity(9.8f)
	, mDebug( 0 )
	, mGroundFriction(0.0013f)
	, mAirFriction(0.00013f)
	, mSteeringAngle( PI2*0.7f )
	, mfFlipForce( 1000.0f )
{
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* ShipControllerData::CreateComponent(Entity* pent) const
{
	ShipControllerInst* sci = OrkNew ShipControllerInst( *this, pent );
	return sci;
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
#endif
