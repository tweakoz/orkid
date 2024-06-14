////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>

#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/ecs/physics/bullet.h>
#include <ork/util/logger.h>

#include "bullet_impl.h"

///////////////////////////////////////////////////////////////////////////////

static const bool USE_GIMPACT = true;
static constexpr bool DEBUG_LOG = true;

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::BulletSystemData, "EcsBulletSystemData");
ImplementReflectionX(ork::ecs::BulletSystem, "EcsBulletSystem");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////


static logchannel_ptr_t logchan_bull = logger()->createChannel("ecs.bulletphy",fvec3(.8,1,.3));

void bulletDebugEnqueueToLayer(ork::lev2::drawablebufitem_constptr_t cdb);
void bulletDebugRender(const ork::lev2::RenderContextInstData& RCID);

///////////////////////////////////////////////////////////////////////////////

void BulletSystemData::describeX(SystemDataClass* clazz) {
  
  clazz->directProperty("SimulationRate", &BulletSystemData::mSimulationRate)
    ->annotate<float>("editor.range.min", 60.0f)
    ->annotate<float>("editor.range.max", 2400.0f);

  clazz->directProperty("TimeScale", &BulletSystemData::mfTimeScale)
    ->annotate<float>("editor.range.min", 0.0f)
    ->annotate<float>("editor.range.max", 50.0f);

  clazz->directProperty("LinGravity", &BulletSystemData::_lingravity);
  clazz->directProperty("ExpGravity", &BulletSystemData::_expgravity);
  clazz->directProperty("Debug", &BulletSystemData::_debug);

}

///////////////////////////////////////////////////////////////////////////////

BulletSystemData::BulletSystemData() {
    _expgravity = fvec3(0,-9.8,0);
}

///////////////////////////////////////////////////////////////////////////////

System* BulletSystemData::createSystem(Simulation* pinst) const {
  return new BulletSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::describeX(object::ObjectClass* clazz) {

}

BulletSystem::BulletSystem(const BulletSystemData& data, Simulation* psi)
    : System(&data, psi)
    , mDynamicsWorld(nullptr)
    , mBtConfig(nullptr)
    , mBroadPhase(nullptr)
    , mDispatcher(nullptr)
    , mSolver(nullptr)
    , _systemData(data)
    , mMaxSubSteps(128)
    , mNumSubStepsTaken(0)
    , mfAvgDtAcc(0.0f)
    , mfAvgDtCtr(0.0f) {
  AllocationLabel("BulletSystem::BulletSystem");

  _debugger = new PhysicsDebugger;

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

  if(_debugger){
    delete _debugger;
  }
  OrkHeapCheck();
}

///////////////////////////////////////////////////////////////////////////////

btDynamicsWorld* BulletSystem::BulletWorld() { //
  return mDynamicsWorld;
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onLinkComponent(BulletObjectComponent* component){
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onUnlinkComponent(BulletObjectComponent* component){

}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onStageComponent(BulletObjectComponent* component){

}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onUnstageComponent(BulletObjectComponent* component){

}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onActivateComponent(BulletObjectComponent* component){
    auto entity = component->GetEntity();

    const BulletSystemData& world_data = this->GetWorldData();
    const auto& CDATA = component->data();

    btVector3 grav                     = orkv3tobtv3(world_data.GetGravity());

    if (btDynamicsWorld* world = this->GetDynamicsWorld()) {
      auto shapedata = component->data()._shapedata;

      if(DEBUG_LOG){
        logchan_bull->log("BulletObjectComponent<%p> shapedata<%p>", (void*)this, (void*)shapedata.get());
      }

      // printf( "SHAPEDATA<%p>\n", shapedata );
      if (shapedata) {
        ShapeCreateData shape_create_data;
        shape_create_data.mEntity = entity;
        shape_create_data.mWorld  = this;
        shape_create_data.mObject = component;

        auto shapeinst = shapedata->CreateShape(shape_create_data);
        component->_shapeinst = shapeinst;

        btCollisionShape* pshape = shapeinst->_collisionShape;

        if (pshape) {
          //_simulation->debugBanner(255, 128, 0, "BulletObjectComponent<%p> pshape<%p>\n", (void*)this, (void*)pshape);
          ////////////////////////////////
          // Initial Transform
          ////////////////////////////////
          auto xform = shape_create_data.mEntity->transform();
          auto P = xform->_translation;
          //printf( "INITIAL<%g %g %g>\n", P.x, P.y, P.z );
          fmtx4 mtx  = xform->composed();
          btTransform btTrans = orkmtx4tobtmtx4(mtx);
          ////////////////////////////////
          auto rigid_body = this->AddLocalRigidBody(shape_create_data.mEntity, CDATA._mass, btTrans, pshape);
          rigid_body->setGravity(grav);
          bool ballowsleep = CDATA._allowSleeping;
          if (CDATA._isKinematic) {
            rigid_body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
            ballowsleep = false;
          }
          rigid_body->setActivationState(ballowsleep ? WANTS_DEACTIVATION : DISABLE_DEACTIVATION);
          rigid_body->activate();
          if(DEBUG_LOG){
            logchan_bull->log("BulletObjectComponent<%p> rigid_body<%p>", (void*) component, (void*)rigid_body);
          }
          component->_rigidbody          = rigid_body;
        }
      }

      auto& forces = component->_forces;

      for (auto it : forces) {
        auto forcecontroller = it.second;
        if (forcecontroller) {
          forcecontroller->DoLink(_simulation);
        }
      }

    }
  _activeComponents.insert(component);
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onDeactivateComponent(BulletObjectComponent* component){
  auto it = _activeComponents.find(component);

  const BulletSystemData& world_data = this->GetWorldData();
  btVector3 grav                     = orkv3tobtv3(world_data.GetGravity());
  if (btDynamicsWorld* world = this->GetDynamicsWorld()) {
    auto rigid_body = component->_rigidbody;
    if (rigid_body) {
      world->removeRigidBody(rigid_body);
      delete rigid_body;
      component->_rigidbody = nullptr;
    }
  }

  _activeComponents.erase(it);
}

///////////////////////////////////////////////////////////////////////////////

static void BulletSystemInternalTickCallback(btDynamicsWorld* world, btScalar timeStep) {
   //printf( "BulletSystemInternalTickCallback( ) timeStep<%f>\n", timeStep );
  OrkAssert(world);

  btDiscreteDynamicsWorld* dynaworld = (btDiscreteDynamicsWorld*)world;

  auto sim = reinterpret_cast<Simulation*>(world->getWorldUserInfo());
  auto bulletsys = sim->findSystem<BulletSystem>();
  //dynaworld->applyGravity();

  ////////////////////////////////////////
  // update bullet family components
  //  at bullet tick rate (independent from scene tick rate)
  ////////////////////////////////////////
  auto exp_grav = orkv3tobtv3(bulletsys->_systemData._expgravity);

  for( BulletObjectComponent* component : bulletsys->_activeComponents ){
    component->update(sim, timeStep);
    if(component->_rigidbody){
      component->_rigidbody->applyCentralForce(exp_grav);
    }
  }
  
}

///////////////////////////////////////////////////////////////////////////////

btRigidBody*
BulletSystem::AddLocalRigidBody(Entity* pent, btScalar mass, const btTransform& startTransform, btCollisionShape* shape) {
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

  auto G = _systemData.GetGravity();
  auto GB = orkv3tobtv3(G);

  //float Gsquared = _systemData.GetGravity().magnitudeSquared();

  //printf( "G<%g %g %g>\n", G.x, G.y, G.z );

  //body->setGravity(GB);
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
  mDynamicsWorld->setInternalTickCallback(BulletSystemInternalTickCallback, _simulation);
  mDynamicsWorld->setDebugDrawer(_debugger);
  //auto G = orkv3tobtv3(_systemData.GetGravity());
  //mDynamicsWorld->setGravity(G);

  if(DEBUG_LOG){
    logchan_bull->log("BulletSystem<%p> mDynamicsWorld<%p>", (void*)this, (void*) mDynamicsWorld );
  }
}

///////////////////////////////////////////////////////////////////////////////

bool BulletSystem::_onLink(Simulation* psi) {

  auto drw = std::make_shared<lev2::CallbackDrawable>(nullptr);
  _debugDrawable = drw;

  drw->setEnqueueOnLayerCallback(bulletDebugEnqueueToLayer);
  drw->SetRenderCallback(bulletDebugRender);
  drw->_sortkey = (0x3fffffff);


  auto pdata       = new BulletDebugDrawDBData(_debugger);
  pdata->_debugger = _debugger;
  _debugDrawable->SetUserDataA(pdata);
  _debugDrawable->_name = "bulletphysdebugger";

  _sgsystem = psi->findSystem<SceneGraphSystem>();
  OrkAssert(_sgsystem!=nullptr);

  _sgsystem->_addStaticDrawable("Default",_debugDrawable);

  return true;
}

void BulletSystem::_onGpuInit(Simulation* psi, lev2::Context* ctx) {
  _debugger->_onGpuInit(psi,ctx);
}
void BulletSystem::_onGpuExit(Simulation* psi, lev2::Context* ctx) {
  _debugger->_onGpuExit(psi,ctx);
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onBeginRender() {
  _debugger->beginRenderFrame();
}
void BulletSystem::_onEndRender() {
  _debugger->endRenderFrame();
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onUpdate(Simulation* inst) {
  if (mDynamicsWorld) {
    float dt = inst->deltaTime();
    _fdtaccum += dt;

    float fps = 1.0f / _fdtaccum;

    float fdts  = dt * _systemData.GetTimeScale();
    float frate = _systemData.GetSimulationRate();

    float ffts = 1.0f / frate;

    //printf("frate<%g> fdts<%g> fps<%g> mMaxSubSteps<%d>\n", frate, fdts, fps, mMaxSubSteps );

    bool is_debug = _systemData.IsDebug();

    _debugger->SetDebug(is_debug);

    if(is_debug)
      _debugger->beginSimFrame(this);

    if (mMaxSubSteps > 0) {

      int a = mDynamicsWorld->stepSimulation(fdts, mMaxSubSteps, ffts);
      int b = mMaxSubSteps;
      int m = std::min(a, b); // ? a : b; // ork::min()
      mNumSubStepsTaken += m;
    }

    if(is_debug)
      _debugger->endSimFrame(this);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
