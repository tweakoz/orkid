////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/bullet.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <Extras/GIMPACTUtils/btGImpactConvexDecompositionShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>

#include<ork/math/PIDController.h>


///////////////////////////////////////////////////////////////////////////////

static const bool USE_GIMPACT = true;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletSystemData, "BulletSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void BulletDebugQueueToLayerCallback(ork::ent::DrawableBufItem&cdb);
void BulletDebugRenderCallback(	ork::lev2::RenderContextInstData& rcid,
							ork::lev2::GfxTarget* targ,
							const ork::lev2::CallbackRenderable* pren );

static PoolString sBulletFamily;

///////////////////////////////////////////////////////////////////////////////

void BulletSystemData::Describe()
{
	ork::reflect::RegisterProperty("SimulationRate", &BulletSystemData::mSimulationRate);
	ork::reflect::AnnotatePropertyForEditor< BulletSystemData >("SimulationRate", "editor.range.min", "60.0f");
	ork::reflect::AnnotatePropertyForEditor< BulletSystemData >("SimulationRate", "editor.range.max", "2400.0f");

	ork::reflect::RegisterProperty("TimeScale", &BulletSystemData::mfTimeScale);
	ork::reflect::AnnotatePropertyForEditor< BulletSystemData >("TimeScale", "editor.range.min", "0.0f");
	ork::reflect::AnnotatePropertyForEditor< BulletSystemData >("TimeScale", "editor.range.max", "50.0f");

	ork::reflect::RegisterProperty("Gravity", &BulletSystemData::mGravity);

	ork::reflect::RegisterProperty("Debug", &BulletSystemData::mbDEBUG);

	sBulletFamily = ork::AddPooledLiteral("bullet");
}

///////////////////////////////////////////////////////////////////////////////

BulletSystemData::BulletSystemData()
		: mfTimeScale(1.0f)
		, mbDEBUG(false)
		, mSimulationRate(120.0f) {

}

///////////////////////////////////////////////////////////////////////////////

System* BulletSystemData::createSystem(ork::ent::SceneInst *pinst) const
{
	return OrkNew BulletSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

BulletSystem::BulletSystem(const BulletSystemData& data,ork::ent::SceneInst* psi)
	: System( &data, psi )
	, mDynamicsWorld(0)
	, mBtConfig( 0 )
	, mBroadPhase(0)
	, mDispatcher(0)
	, mSolver(0)
	, mBWCBD(data)
	, mMaxSubSteps(128)
	, mNumSubStepsTaken(0)
	, mfAvgDtAcc(0.0f)
	, mfAvgDtCtr(0.0f)
{
	AllocationLabel("BulletSystem::BulletSystem");
	InitWorld();
}

///////////////////////////////////////////////////////////////////////////////

BulletSystem::~BulletSystem()
{
	if( mDynamicsWorld ) delete mDynamicsWorld;
	if( mSolver ) delete mSolver;
	if( mBtConfig ) delete mBtConfig;
	if( mDispatcher ) delete mDispatcher;
	if( mBroadPhase ) delete mBroadPhase;

	OrkHeapCheck();
}


///////////////////////////////////////////////////////////////////////////////

static void BulletSystemInternalTickCallback(btDynamicsWorld *world, btScalar timeStep)
{
	//printf( "BulletSystemInternalTickCallback( ) timeStep<%f>\n", timeStep );
	OrkAssert(world);

	btDiscreteDynamicsWorld* dynaworld = (btDiscreteDynamicsWorld*) world;

	ork::ent::SceneInst *sinst = reinterpret_cast<ork::ent::SceneInst *>(world->getWorldUserInfo());

	float previous_dt = sinst->GetDeltaTime();

	dynaworld->applyGravity();

	////////////////////////////////////////
	// update bullet family components
	//  at bullet tick rate (independent from scene tick rate)
	////////////////////////////////////////

	sinst->SetDeltaTime(timeStep);
		sinst->UpdateActiveComponents(sBulletFamily);
	sinst->SetDeltaTime(previous_dt);

}

///////////////////////////////////////////////////////////////////////////////

btRigidBody* BulletSystem::AddLocalRigidBody(ork::ent::Entity* pent
														  , btScalar mass, const btTransform& startTransform
														  , btCollisionShape* shape)
{
	OrkAssert(pent);

    // rigidbody is dynamic if and only if mass is non zero, otherwise static
    bool isDynamic = (mass != 0.f);

    btVector3 localInertia(0, 0, 0);
    if(isDynamic && shape)
		shape->calculateLocalInertia(mass,localInertia);

	btMotionState *motionstate = new EntMotionState(startTransform, pent);

    btRigidBody::btRigidBodyConstructionInfo cInfo(mass, motionstate, shape, localInertia);

    btRigidBody *body = new btRigidBody(cInfo);
	body->setUserPointer(pent);
	body->setRestitution(1.0f);
	body->setFriction(1.0f);
	//body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
	body->setUserPointer(pent);

    mDynamicsWorld->addRigidBody(body);

	if( mBWCBD.GetGravity().MagSquared() == 0.0f )
	{
		body->setGravity(btVector3(0.0f, 0.0f, 0.0f));
	}
    return body;
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::InitWorld()
{
	btDefaultCollisionConstructionInfo cinfo;
	cinfo.m_stackAlloc = 0;
	cinfo.m_persistentManifoldPool = 0;
	cinfo.m_collisionAlgorithmPool = 0;

	cinfo.m_defaultMaxPersistentManifoldPoolSize = 4096;
	cinfo.m_defaultMaxCollisionAlgorithmPoolSize = 4096;
	cinfo.m_defaultStackAllocatorSize = 64<<20; // 2MB

	// collision configuration contains default setup for memory, collision setup
	mBtConfig = new btDefaultCollisionConfiguration(cinfo);

	// use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	mDispatcher = new btCollisionDispatcher(mBtConfig);

	if( USE_GIMPACT )
	{
		btGImpactCollisionAlgorithm::registerAlgorithm(mDispatcher);
	}

	// the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
	mSolver = new btSequentialImpulseConstraintSolver;

	btDbvtBroadphase *broadphase = new btDbvtBroadphase();
	// OR
	//btVector3 worldMin(-1000,-1000,-1000);
	//btVector3 worldMax(1000,1000,1000);
	//btAxisSweep3 *broadphase = new btAxisSweep3(worldMin, worldMax);

	mBroadPhase = broadphase;

	mDynamicsWorld = new btDiscreteDynamicsWorld(mDispatcher, mBroadPhase, mSolver, mBtConfig);
	mDynamicsWorld->getSolverInfo().m_solverMode &= ~SOLVER_RANDMIZE_ORDER;

	mDynamicsWorld->setGravity( ! mBWCBD.GetGravity() );
	orkprintf( "mDynamicsWorld<%p>\n", mDynamicsWorld );

}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::LinkPhysics(ork::ent::SceneInst *inst, ork::ent::Entity *pent)
{
	printf( "LINKING physics\n" );

	mDynamicsWorld->setInternalTickCallback(BulletSystemInternalTickCallback, inst);

	#if 1 //DRAWTHREADS
	ork::ent::CallbackDrawable* pdrw = new ork::ent::CallbackDrawable( pent );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetSortKey(0x7fffffff);
	pent->AddDrawable( AddPooledLiteral("Default"), pdrw );
	pdrw->SetRenderCallback( BulletDebugRenderCallback );
	pdrw->SetQueueToLayerCallback( BulletDebugQueueToLayerCallback );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetSortKey(0x3fffffff);

	BulletDebugDrawDBData* pdata = new BulletDebugDrawDBData( this, pent );
	pdrw->SetUserDataA( pdata );

	pdata->mpDebugger = & mDebugger;
	mDynamicsWorld->setDebugDrawer( & mDebugger );

	btVector3 grav = !mBWCBD.GetGravity();
	mDynamicsWorld->setGravity(grav);
	#endif

}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::DoUpdate(ork::ent::SceneInst* inst)
{
	if(mDynamicsWorld)
	{
		float dt = inst->GetDeltaTime();

		static float fdtaccum = 0.0f;

		float fps = 1.0f / fdtaccum;

		float fdts = dt * mBWCBD.GetTimeScale();
		float frate = mBWCBD.GetSimulationRate();

		float ffts = 1.0f / frate;

		mDebugger.SetDebug(mBWCBD.IsDebug());

		if(mMaxSubSteps > 0)
		{

			int a = mDynamicsWorld->stepSimulation(fdts, mMaxSubSteps, ffts);
			int b = mMaxSubSteps;
			int m = std::min(a,b);// ? a : b; // ork::min()
			mNumSubStepsTaken += m;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} } // namespace ork { namespace ent
