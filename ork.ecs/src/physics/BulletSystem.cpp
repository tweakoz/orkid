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

#include <ork/ecs/component.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/ecs/physics/bullet.h>
#include <ork/util/logger.h>
#include <ork/ecs/datatable.h>

#include "bullet_impl.h"
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////

static const bool USE_GIMPACT   = true;
static constexpr bool DEBUG_LOG = false;

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::BulletSystemData, "EcsBulletSystemData");
ImplementReflectionX(ork::ecs::BulletSystem, "EcsBulletSystem");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_bull = logger()->createChannel("ecs.bulletphy", fvec3(.8, 1, .3));

void bulletDebugEnqueueToLayer(ork::lev2::drawqueueitem_constptr_t cdb);
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
  _expgravity = fvec3(0, -9.8, 0);
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

  if (_debugger) {
    delete _debugger;
  }
  OrkHeapCheck();
}

///////////////////////////////////////////////////////////////////////////////

btDynamicsWorld* BulletSystem::BulletWorld() { //
  return mDynamicsWorld;
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onLinkComponent(BulletObjectComponent* component) {
  auto entity       = component->GetEntity();
  auto compdata     = &component->mBOCD;
  auto instancedata = compdata->_INSTANCEDATA;
  if (instancedata) {
    auto sgcomp = entity->typedComponent<SceneGraphComponent>();
    // if both SG and physics instancedatas are the same
    //  then both components are referencing to the same instance
    if (sgcomp and (sgcomp->_SGCD._INSTANCEDATA == instancedata)) {
      component->_mySGcomponentForInstancing = sgcomp;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onUnlinkComponent(BulletObjectComponent* component) {
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onStageComponent(BulletObjectComponent* component) {
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onUnstageComponent(BulletObjectComponent* component) {
  auto entity = component->GetEntity();
  auto sgcomp = entity->typedComponent<SceneGraphComponent>();
  if (sgcomp->_INSTANCE) {
    // free instance
  }
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onActivateComponent(BulletObjectComponent* component) {
  auto entity                        = component->GetEntity();
  btRigidBody* rigid_body            = nullptr;
  auto compdata                      = &component->mBOCD;
  _lastcomponentfordata[compdata]    = component;
  component->_birthtime              = _simulation->gameTime();
  const BulletSystemData& world_data = this->GetWorldData();
  const auto& CDATA                  = component->data();

  btVector3 grav = orkv3tobtv3(world_data.GetGravity());

  bool wants_update = true;

  if (btDynamicsWorld* world = this->GetDynamicsWorld()) {
    auto shapedata = component->data()._shapedata;

    if (DEBUG_LOG) {
      logchan_bull->log("BulletObjectComponent<%p> shapedata<%p>", (void*)this, (void*)shapedata.get());
    }

    // printf( "SHAPEDATA<%p>\n", shapedata );
    if (shapedata) {
      ShapeCreateData shape_create_data;
      shape_create_data.mEntity = entity;
      shape_create_data.mWorld  = this;
      shape_create_data.mObject = component;

      auto shapeinst        = shapedata->CreateShape(shape_create_data);
      component->_shapeinst = shapeinst;

      btCollisionShape* pshape = shapeinst->_collisionShape;

      if (pshape) {
        //_simulation->debugBanner(255, 128, 0, "BulletObjectComponent<%p> pshape<%p>\n", (void*)this, (void*)pshape);
        ////////////////////////////////
        // Initial Transform
        ////////////////////////////////
        auto xform = shape_create_data.mEntity->transform();
        auto P     = xform->_translation;
        // printf( "INITIAL<%g %g %g>\n", P.x, P.y, P.z );
        fmtx4 mtx           = xform->composed();
        btTransform btTrans = orkmtx4tobtmtx4(mtx);
        ////////////////////////////////
        float mass = CDATA._mass;
        if (entity->_spawnanondata) {
          auto spawndata = entity->_spawnanondata;
          auto table     = spawndata->_table;
          if (table) {
            auto massprop = (*table)["mass"_crcu];
            if (auto as_float = massprop.tryAs<float>()) {
              mass = as_float.value();
            }
          }
        }
        ////////////////////////////////
        rigid_body =
            this->AddLocalRigidBody(shape_create_data.mEntity, mass, btTrans, pshape, CDATA._groupAssign, CDATA._groupCollidesWith);

        if (CDATA._collisionCallback != nullptr) {
          auto collision_tester         = std::make_shared<OrkContactResultCallback>(rigid_body);
          collision_tester->_onContact  = CDATA._collisionCallback;
          component->_collisionCallback = collision_tester;
          collision_tester->_system     = this;
          _collisionCallbacks.insert(collision_tester);
        }

        rigid_body->setGravity(grav);
        bool ballowsleep = CDATA._allowSleeping;
        if (CDATA._isKinematic) {
          rigid_body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
          ballowsleep = false;
        }
        rigid_body->setActivationState(ballowsleep ? WANTS_DEACTIVATION : DISABLE_DEACTIVATION);
        rigid_body->activate();
        if (DEBUG_LOG) {
          logchan_bull->log("BulletObjectComponent<%p> rigid_body<%p>", (void*)component, (void*)rigid_body);
        }
        component->_rigidbody = rigid_body;

        if (component->mBOCD._angularFactor.magnitude() > 0.1f) {
          rigid_body->setAngularFactor(orkv3tobtv3(component->mBOCD._angularFactor));
        }
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

  if (CDATA._disablePhysics)
    wants_update = false;
  if (not component->_rigidbody)
    wants_update = false;

  if (wants_update) {
    if (component->_forces.size() > 0) {
      _updateForceComponents.insert(component);
    }
    if (CDATA._isKinematic) {
      _updateKinematicComponents.insert(component);
    } else {
      _updateDynamicComponents.insert(component);
      _updateCheckComponents.insert(component);
    }
  }

  // instance tracking (WIP)
  if (component->_mySGcomponentForInstancing) {
    component->_mySGcomponentForInstancing->_onInstanceCreated = [=]() {
      // at this point this entity's SG component should be staged
      // and therefore SG component's _INSTANCE should be already created
      auto instance = component->_mySGcomponentForInstancing->_INSTANCE;
      OrkAssert(instance);
      component->_sginstance_id = instance->_instance_index;
      OrkAssert(component->_sginstance_id >= 0);
      auto idata                   = instance->_idata;
      auto entmotionstate          = (EntMotionState*)component->_rigidbody->getMotionState();
      entmotionstate->_instance_id = component->_sginstance_id;
      entmotionstate->_idata       = idata;
    };
  }
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onDeactivateComponent(BulletObjectComponent* component) {
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

  _updateForceComponents.remove(component);
  _updateKinematicComponents.remove(component);
  _updateDynamicComponents.remove(component);
}

///////////////////////////////////////////////////////////////////////////////

static void BulletSystemInternalTickCallback(btDynamicsWorld* world, btScalar timeStep) {
  OrkAssert(world);
  btDiscreteDynamicsWorld* dynaworld = (btDiscreteDynamicsWorld*)world;
  auto sim                           = reinterpret_cast<Simulation*>(world->getWorldUserInfo());
  auto bulletsys                     = sim->findSystem<BulletSystem>();
  dynaworld->applyGravity();
}

///////////////////////////////////////////////////////////////////////////////

btRigidBody* BulletSystem::AddLocalRigidBody(
    Entity* pent,                      //
    btScalar mass,                     //
    const btTransform& startTransform, //
    btCollisionShape* shape,
    uint32_t group_assign,
    uint32_t groups_collides_with) { //
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

  mDynamicsWorld->addRigidBody(body, group_assign, groups_collides_with);

  auto G  = _systemData.GetGravity();
  auto GB = orkv3tobtv3(G);

  // float Gsquared = _systemData.GetGravity().magnitudeSquared();

  // printf( "G<%g %g %g>\n", G.x, G.y, G.z );

  // body->setGravity(GB);

  return body;
}

///////////////////////////////////////////////////////////////////////////////

OrkContactResultCallback::OrkContactResultCallback(btRigidBody* body) //
    : monitoredBody(body) {                                           //
}

btScalar OrkContactResultCallback::addSingleResult(
    btManifoldPoint& cp,
    const btCollisionObjectWrapper* colObj0Wrap,
    int partId0,
    int index0,
    const btCollisionObjectWrapper* colObj1Wrap,
    int partId1,
    int index1) {
  btRigidBody* body0 = (btRigidBody*)colObj0Wrap->getCollisionObject();
  btRigidBody* body1 = (btRigidBody*)colObj1Wrap->getCollisionObject();

  if (body0 == monitoredBody || body1 == monitoredBody) {

    if (_onContact) {
      const btCollisionObject* colObj0 = colObj0Wrap->getCollisionObject();
      const btCollisionObject* colObj1 = colObj1Wrap->getCollisionObject();

      const btVector3& ptA       = cp.getPositionWorldOnA();
      const btVector3& ptB       = cp.getPositionWorldOnB();
      const btVector3& normalOnB = cp.m_normalWorldOnB;
      int group0                 = colObj0->getBroadphaseHandle()->m_collisionFilterGroup;
      int group1                 = colObj1->getBroadphaseHandle()->m_collisionFilterGroup;

      auto invocation = std::make_shared<deferred_script_invokation>();

      invocation->_cb = _onContact;

      auto& datatable            = *invocation->_data.makeShared<DataTable>();
      auto entA                  = (Entity*)body0->getUserPointer();
      auto entB                  = (Entity*)body1->getUserPointer();
      EntityRef erefA            = {entA->_entref};
      EntityRef erefB            = {entB->_entref};
      datatable["entityA"_tok]   = entA;
      datatable["entityB"_tok]   = entB;
      datatable["entrefA"_tok]   = erefA;
      datatable["entrefB"_tok]   = erefB;
      datatable["groupA"_tok]    = group0;
      datatable["groupB"_tok]    = group1;
      datatable["pointA"_tok]    = btv3toorkv3(ptA);
      datatable["pointB"_tok]    = btv3toorkv3(ptB);
      datatable["normalOnB"_tok] = btv3toorkv3(normalOnB).normalized();

      auto sim = _system->simulation();
      sim->_enqueueDeferredInvokation(invocation);
    }
  }

  return 0;
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::InitWorld() {
  btDefaultCollisionConstructionInfo cinfo;
  // cinfo.m_stackAlloc = 0;
  cinfo.m_persistentManifoldPool = 0;
  cinfo.m_collisionAlgorithmPool = 0;

  cinfo.m_defaultMaxPersistentManifoldPoolSize = 16384;
  cinfo.m_defaultMaxCollisionAlgorithmPoolSize = 16384;
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
  // mDynamicsWorld->getSolverInfo().m_solverMode &= ~SOLVER_RANDMIZE_ORDER;
  mDynamicsWorld->getSolverInfo().m_solverMode |= SOLVER_CACHE_FRIENDLY;
  mDynamicsWorld->getSolverInfo().m_solverMode |= SOLVER_SIMD;
  mDynamicsWorld->getSolverInfo().m_numIterations = 30;
  mDynamicsWorld->setInternalTickCallback(BulletSystemInternalTickCallback, _simulation);
  mDynamicsWorld->setDebugDrawer(_debugger);
  // auto G = orkv3tobtv3(_systemData.GetGravity());
  // mDynamicsWorld->setGravity(G);

  if (DEBUG_LOG) {
    logchan_bull->log("BulletSystem<%p> mDynamicsWorld<%p>", (void*)this, (void*)mDynamicsWorld);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool BulletSystem::_onLink(Simulation* psi) {

  auto drw       = std::make_shared<lev2::CallbackDrawable>(nullptr);
  _debugDrawable = drw;

  drw->setEnqueueOnLayerCallback(bulletDebugEnqueueToLayer);
  drw->SetRenderCallback(bulletDebugRender);
  drw->_sortkey = (0x3fffffff);

  auto pdata       = new BulletDebugDrawDBData(_debugger);
  pdata->_debugger = _debugger;
  _debugDrawable->SetUserDataA(pdata);
  _debugDrawable->_name = "bulletphysdebugger";

  _sgsystem = psi->findSystem<SceneGraphSystem>();
  OrkAssert(_sgsystem != nullptr);

  _sgsystem->_addStaticDrawable("Default", _debugDrawable);

  return true;
}

void BulletSystem::_onGpuInit(Simulation* psi, lev2::Context* ctx) {
  _debugger->_onGpuInit(psi, ctx);
}
void BulletSystem::_onGpuLink(Simulation* psi, lev2::Context* ctx) {

  for (auto component : _activeComponents) {
    auto iname = component->mBOCD._instanceNodeName;
    if (iname != "") {
      auto ent = component->GetEntity();
      OrkAssert(ent != nullptr);
      auto sgsys = simulation()->findSystem<SceneGraphSystem>();
      OrkAssert(sgsys != nullptr);
      auto itnode = sgsys->_nodeitems.find(iname);
      OrkAssert(itnode != sgsys->_nodeitems.end());
    }
  }
}
void BulletSystem::_onGpuExit(Simulation* psi, lev2::Context* ctx) {
  _debugger->_onGpuExit(psi, ctx);
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
  EASY_BLOCK("BulletSystem::_onUpdate", profiler::colors::Cyan);
  if (mDynamicsWorld) {
    float dt = inst->deltaTime();
    float gt = inst->gameTime();
    _fdtaccum += dt;

    float fps = 1.0f / _fdtaccum;

    float fdts  = dt * _systemData.GetTimeScale();
    float frate = _systemData.GetSimulationRate();

    float ffts = 1.0f / frate;

    // printf("frate<%g> fdts<%g> fps<%g> mMaxSubSteps<%d>\n", frate, fdts, fps, mMaxSubSteps );

    bool is_debug = _systemData.IsDebug();

    _debugger->SetDebug(is_debug);

    if (is_debug)
      _debugger->beginSimFrame(this);

    if (mMaxSubSteps > 0) {

      EASY_BLOCK("kinematic", profiler::colors::Cyan);
      for (BulletObjectComponent* component : _updateKinematicComponents._linear) {
        component->updateKinematic(_simulation, dt);
      }
      EASY_END_BLOCK;
      EASY_BLOCK("dynamic", profiler::colors::Cyan);
      for (BulletObjectComponent* component : _updateDynamicComponents._linear) {
        component->updateDynamic(_simulation, dt);
      }
      EASY_END_BLOCK;
      EASY_BLOCK("forces", profiler::colors::Cyan);
      for (BulletObjectComponent* component : _updateForceComponents._linear) {
        component->updateForces(_simulation, dt);
      }
      EASY_END_BLOCK;

      /////////////////////////////////////////
      // initial sleep at 10 seconds of age
      /////////////////////////////////////////

      if (_systemData._test_deactivation) {
        size_t count = _updateCheckComponents._linear.size();
        if (count) {
          _deactivation_queue.clear();
          size_t num_to_check = 500;
          size_t num          = (count < num_to_check) ? count : num_to_check;
          for (size_t i = 0; i < num; i++) {
            size_t rand_item = rand() % count;
            auto comp        = _updateCheckComponents._linear[rand_item];
            auto motstate    = (EntMotionState*)comp->_rigidbody->getMotionState();
            double energy    = motstate->_energy.length();
            // printf( "energy<%g>\n", energy);
            float age = gt - comp->_birthtime;
            if (energy < 1 and age > 15.0) {
              // printf("locking rbody<%p> energy<%g>\n", (void*)comp->_rigidbody, motstate->_energy.length());
              auto rbody      = comp->_rigidbody;
              auto prev_state = rbody->getActivationState();
              auto prev_flags = rbody->getCollisionFlags();
              rbody->setActivationState(prev_state | ISLAND_SLEEPING | DISABLE_DEACTIVATION);
              rbody->setCollisionFlags(prev_flags | btCollisionObject::CF_KINEMATIC_OBJECT);
              mDynamicsWorld->removeRigidBody(rbody);

              btScalar mass = 0.0;             // Set mass to 0 for a static object
              btVector3 localInertia(0, 0, 0); // No inertia for static objects

              rbody->setMassProps(mass, localInertia);
              rbody->updateInertiaTensor();

              mDynamicsWorld->addRigidBody(rbody);
              _deactivation_queue.insert(comp);
              motstate->_permadeactived = true;
              auto ent                  = comp->GetEntity();
              auto sgcomp               = ent->typedComponent<SceneGraphComponent>();
              sgcomp->_onNotify(inst, "ChangeModColor"_crcu, fvec4(1, 1, 1, 1));
            }
          }
        }
        /////////////////////////////////////////
        for (auto comp : _deactivation_queue) {
          _updateCheckComponents.remove(comp);
          _updateForceComponents.remove(comp);
          _sleptDynamicComponents.insert(comp);
        }
        _deactivation_queue.clear();
        /////////////////////////////////////////
        // permanent sleep at 30 seconds of age
        /////////////////////////////////////////
        count               = _sleptDynamicComponents._linear.size();
        size_t num_to_check = 100;
        size_t num          = (count < num_to_check) ? count : num_to_check;
        for (size_t i = 0; i < num; i++) {
          size_t rand_item = rand() % count;
          auto comp        = _sleptDynamicComponents._linear[rand_item];
          auto motstate    = (EntMotionState*)comp->_rigidbody->getMotionState();
          motstate->_counter++;
          float age = gt - comp->_birthtime;
          if (age > 40.0f) {
            _deactivation_queue.insert(comp);
          }
        }
        /////////////////////////////////////////
        for (auto comp : _deactivation_queue) {
          _sleptDynamicComponents.remove(comp);
          auto rbody = comp->_rigidbody;
          // printf("removing rbody<%p> \n", (void*)rbody);
          auto ent    = comp->GetEntity();
          auto sgcomp = ent->typedComponent<SceneGraphComponent>();
          sgcomp->_onNotify(inst, "ChangeModColor"_crcu, fvec4(0, 0, 0, 1));
          // mDynamicsWorld->removeRigidBody(rbody);
        }
        /////////////////////////////////////////
      }

      EASY_BLOCK("simulation", profiler::colors::Cyan);
      int a = mDynamicsWorld->stepSimulation(fdts, mMaxSubSteps, ffts);
      int b = mMaxSubSteps;
      int m = std::min(a, b); // ? a : b; // ork::min()
      mNumSubStepsTaken += m;
      EASY_END_BLOCK;

      EASY_BLOCK("collisions", profiler::colors::Cyan);
      for (auto callback : _collisionCallbacks) {
        auto body = callback->monitoredBody;
        mDynamicsWorld->contactTest(body, *callback);
      }
      EASY_END_BLOCK;
    }

    if (is_debug)
      _debugger->endSimFrame(this);
  }
}

void BulletSystem::_onNotify(token_t evID, evdata_t data) {

  switch (evID.hashed()) {
    case "IMPULSE_ON_COMPONENT_DATA"_crcu: {
      const auto& table = *data.getShared<DataTable>();
      auto compdata     = table["component"_tok].get<bulletobjectcomponentdata_ptr_t>();
      printf("compdata<%p>\n", compdata.get());
      auto it = _lastcomponentfordata.find(compdata.get());
      if (it != _lastcomponentfordata.end()) {
        auto component   = it->second;
        auto rigid_body  = component->_rigidbody;
        auto impulse_val = table["impulse"_tok].get<fvec3>();
        rigid_body->applyCentralImpulse(orkv3tobtv3(impulse_val));
      }
      break;
    }
    case "IMPULSE_ON_COMPONENT"_crcu: {
      const auto& table = *data.getShared<DataTable>();
      auto compref      = table["component"_tok].get<comp_ref_t>();
      auto component    = simulation()->_findComponentFromRef(compref);
      auto as_physics   = dynamic_cast<BulletObjectComponent*>(component);
      auto rigid_body   = as_physics->_rigidbody;
      auto impulse_val  = table["impulse"_tok].get<fvec3>();
      // printf( "physc<%p> rbody<%p>\n", (void*) as_physics, (void*) rigid_body );
      rigid_body->applyCentralImpulse(orkv3tobtv3(impulse_val));
      break;
    }
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
