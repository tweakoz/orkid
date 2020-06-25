////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <pkg/ent/bullet.h>
//#include <Extras/GIMPACTUtils/btGImpactConvexDecompositionShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>

#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>

#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>
#include <ork/reflect/properties/DirectMapTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/RegisterProperty.h>

#include <ork/math/PIDController.h>

///////////////////////////////////////////////////////////////////////////////

static const bool USE_GIMPACT = true;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletSystemData, "BulletSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void bulletDebugEnqueueToLayer(ork::lev2::DrawableBufItem& cdb);
void bulletDebugRender(const ork::lev2::RenderContextInstData& RCID);

static PoolString sBulletFamily;

///////////////////////////////////////////////////////////////////////////////

void BulletSystemData::Describe() {
  ork::reflect::RegisterProperty("SimulationRate", &BulletSystemData::mSimulationRate);
  ork::reflect::annotatePropertyForEditor<BulletSystemData>("SimulationRate", "editor.range.min", "60.0f");
  ork::reflect::annotatePropertyForEditor<BulletSystemData>("SimulationRate", "editor.range.max", "2400.0f");

  ork::reflect::RegisterProperty("TimeScale", &BulletSystemData::mfTimeScale);
  ork::reflect::annotatePropertyForEditor<BulletSystemData>("TimeScale", "editor.range.min", "0.0f");
  ork::reflect::annotatePropertyForEditor<BulletSystemData>("TimeScale", "editor.range.max", "50.0f");

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

System* BulletSystemData::createSystem(ork::ent::Simulation* pinst) const {
  return OrkNew BulletSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

BulletSystem::BulletSystem(const BulletSystemData& data, ork::ent::Simulation* psi)
    : System(&data, psi)
    , mDynamicsWorld(0)
    , mBtConfig(0)
    , mBroadPhase(0)
    , mDispatcher(0)
    , mSolver(0)
    , _systemData(data)
    , mMaxSubSteps(128)
    , mNumSubStepsTaken(0)
    , mfAvgDtAcc(0.0f)
    , mfAvgDtCtr(0.0f) {
  AllocationLabel("BulletSystem::BulletSystem");
  InitWorld();
}

///////////////////////////////////////////////////////////////////////////////

BulletSystem::~BulletSystem() {
  if (mDynamicsWorld)
    delete mDynamicsWorld;
  if (mSolver)
    delete mSolver;
  if (mBtConfig)
    delete mBtConfig;
  if (mDispatcher)
    delete mDispatcher;
  if (mBroadPhase)
    delete mBroadPhase;

  OrkHeapCheck();
}

///////////////////////////////////////////////////////////////////////////////

static void BulletSystemInternalTickCallback(btDynamicsWorld* world, btScalar timeStep) {
  // printf( "BulletSystemInternalTickCallback( ) timeStep<%f>\n", timeStep );
  OrkAssert(world);

  btDiscreteDynamicsWorld* dynaworld = (btDiscreteDynamicsWorld*)world;

  ork::ent::Simulation* sinst = reinterpret_cast<ork::ent::Simulation*>(world->getWorldUserInfo());

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

btRigidBody*
BulletSystem::AddLocalRigidBody(ork::ent::Entity* pent, btScalar mass, const btTransform& startTransform, btCollisionShape* shape) {
  OrkAssert(pent);

  // rigidbody is dynamic if and only if mass is non zero, otherwise static
  bool isDynamic = (mass != 0.f);

  btVector3 localInertia(0, 0, 0);
  if (isDynamic && shape)
    shape->calculateLocalInertia(mass, localInertia);

  auto motionstate = new EntMotionState(startTransform, pent);

  btRigidBody::btRigidBodyConstructionInfo cInfo(mass, motionstate, shape, localInertia);

  auto body = new btRigidBody(cInfo);
  body->setUserPointer(pent);
  body->setRestitution(1.0f);
  body->setFriction(1.0f);
  // body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
  body->setUserPointer(pent);

  mDynamicsWorld->addRigidBody(body);

  if (_systemData.GetGravity().MagSquared() == 0.0f) {
    body->setGravity(btVector3(0.0f, 0.0f, 0.0f));
  }
  return body;
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::InitWorld() {
  btDefaultCollisionConstructionInfo cinfo;
  // cinfo.m_stackAlloc = 0;
  cinfo.m_persistentManifoldPool = 0;
  cinfo.m_collisionAlgorithmPool = 0;

  cinfo.m_defaultMaxPersistentManifoldPoolSize = 4096;
  cinfo.m_defaultMaxCollisionAlgorithmPoolSize = 4096;
  // cinfo.m_defaultStackAllocatorSize = 64<<20; // 2MB

  // collision configuration contains default setup for memory, collision setup
  mBtConfig = new btDefaultCollisionConfiguration(cinfo);

  // use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
  mDispatcher = new btCollisionDispatcher(mBtConfig);

  if (USE_GIMPACT) {
    btGImpactCollisionAlgorithm::registerAlgorithm(mDispatcher);
  }

  // the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
  mSolver = new btSequentialImpulseConstraintSolver;

  auto broadphase = new btDbvtBroadphase();
  // OR
  // btVector3 worldMin(-1000,-1000,-1000);
  // btVector3 worldMax(1000,1000,1000);
  // btAxisSweep3 *broadphase = new btAxisSweep3(worldMin, worldMax);

  mBroadPhase = broadphase;

  mDynamicsWorld = new btDiscreteDynamicsWorld(mDispatcher, mBroadPhase, mSolver, mBtConfig);
  mDynamicsWorld->getSolverInfo().m_solverMode &= ~SOLVER_RANDMIZE_ORDER;

  mDynamicsWorld->setGravity(!_systemData.GetGravity());
  orkprintf("mDynamicsWorld<%p>\n", mDynamicsWorld);
}

///////////////////////////////////////////////////////////////////////////////

bool BulletSystem::DoLink(Simulation* psi) {

  _dbgdrawlayername = "Default";

  _debugDrawable = new ork::lev2::CallbackDrawable(nullptr);
  _debugDrawable->SetSortKey(0x7fffffff);
  _debugDrawable->setEnqueueOnLayerCallback(bulletDebugEnqueueToLayer);
  _debugDrawable->SetRenderCallback(bulletDebugRender);
  _debugDrawable->SetOwner(&_systemData);
  _debugDrawable->SetSortKey(0x3fffffff);

  auto pdata       = new BulletDebugDrawDBData(this);
  pdata->_debugger = &_debugger;
  _debugDrawable->SetUserDataA(pdata);

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::enqueueDrawables(lev2::DrawableBuffer& buffer) {

  if (_debugger._enabled) {
    auto buflayer = buffer.MergeLayer(_dbgdrawlayername);
    _debugDrawable->enqueueOnLayer(_dbgdrawXF, *buflayer);
  }
  // do something with _debugDrawable
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::beginRenderFrame(const Simulation* psi) {
  _debugger.beginRenderFrame();
}
void BulletSystem::endRenderFrame(const Simulation* psi) {
  _debugger.endRenderFrame();
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::LinkPhysicsObject(ork::ent::Simulation* inst, ork::ent::Entity* pent) {
  mDynamicsWorld->setInternalTickCallback(BulletSystemInternalTickCallback, inst);

  mDynamicsWorld->setDebugDrawer(&_debugger);

  btVector3 grav = !_systemData.GetGravity();
  mDynamicsWorld->setGravity(grav);
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::DoUpdate(ork::ent::Simulation* inst) {
  if (mDynamicsWorld) {
    float dt = inst->GetDeltaTime();

    static float fdtaccum = 0.0f;

    float fps = 1.0f / fdtaccum;

    float fdts  = dt * _systemData.GetTimeScale();
    float frate = _systemData.GetSimulationRate();

    float ffts = 1.0f / frate;

    _debugger.SetDebug(_systemData.IsDebug());

    _debugger.beginSimFrame(this);

    if (mMaxSubSteps > 0) {

      int a = mDynamicsWorld->stepSimulation(fdts, mMaxSubSteps, ffts);
      int b = mMaxSubSteps;
      int m = std::min(a, b); // ? a : b; // ork::min()
      mNumSubStepsTaken += m;
    }

    _debugger.endSimFrame(this);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
