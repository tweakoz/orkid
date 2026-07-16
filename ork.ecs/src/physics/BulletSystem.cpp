////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <chrono>

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
#include <unordered_map>
#include <ork/kernel/profiler.h>

///////////////////////////////////////////////////////////////////////////////

static const bool USE_GIMPACT   = true;
static constexpr bool DEBUG_LOG = false;

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::BulletSystemData, "EcsBulletSystemData");
ImplementReflectionX(ork::ecs::BulletSystem, "EcsBulletSystem");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_bull = logger()->configureChannel("ecs.bulletphy", fvec3(.8, 1, .3));

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
  if (_overlapFilter)
    delete _overlapFilter;

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

void BulletSystem::_registerTerrainYSpan(float minY, float maxY) {
  if (not _hasTerrainFloor) {
    _hasTerrainFloor = true;
    _terrainMinY     = minY;
    _terrainMaxY     = maxY;
  } else { // union across every terrain collider present
    if (minY < _terrainMinY)
      _terrainMinY = minY;
    if (maxY > _terrainMaxY)
      _terrainMaxY = maxY;
  }
}

///////////////////////////////////////////////////////////////////////////////

void BulletSystem::_onLinkComponent(BulletObjectComponent* component) {
  auto entity       = component->GetEntity();
  auto compdata     = &component->mBOCD;
  auto instancedata = compdata->_INSTANCEDATA;
  // instance pairing is BY GROUP NAME (the serializable contract); the legacy
  // shared-ptr form still resolves through its groupname.
  std::string bname = instancedata ? instancedata->_groupname : compdata->_instanceNodeName;
  if (bname != "") {
    auto sgcomp = entity->typedComponent<SceneGraphComponent>();
    if (sgcomp) {
      const auto& SGCD  = sgcomp->_SGCD;
      std::string sname = SGCD._INSTANCEDATA ? SGCD._INSTANCEDATA->_groupname : SGCD._instanceNodeName;
      if (sname == bname) {
        component->_mySGcomponentForInstancing = sgcomp;
      }
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

        if (CDATA._collisionCallback != nullptr or CDATA._notifyCollisions) {
          auto collision_tester = std::make_shared<OrkContactResultCallback>(rigid_body);
          auto py_cb            = CDATA._collisionCallback;
          bool do_notify        = CDATA._notifyCollisions;
          // E.2-walk: _notifyCollisions forwards each contact to the scene's PythonSystem
          // as a "Collision" notify ({nameA,nameB,pointA,normalOnB,...}) — the system
          // SCRIPT intercepts (onSystemNotify). Runs at deferred-invocation drain time on
          // the update thread (the same in-thread direct-call pattern as camera publish).
          // TRANSITION dedup (the 1-FPS lesson): a resting body contacts EVERY tick
          // (480 UPS) — forwarding each one into the python sub-interpreter (GIL bind +
          // call per manifold point) collapses the update rate. Gameplay wants ENTER
          // events: notify per other-entity only when contact (re)begins, re-armed
          // after a 0.25s touch gap. Persistent contact -> ONE notify, then silence.
          struct NotifyDedup {
            std::unordered_map<uint64_t, float> _lastSeen;
          };
          auto dedup = std::make_shared<NotifyDedup>();
          collision_tester->_onContact = [this, py_cb, do_notify, dedup](const evdata_t& data) {
            if (py_cb)
              py_cb(data);
            if (do_notify) {
              const auto& table = *data.getShared<DataTable>();
              uint64_t other    = table["entrefB"_tok].get<EntityRef>()._entID;
              const float now   = simulation()->gameTime();
              auto it           = dedup->_lastSeen.find(other);
              const bool enter  = (it == dedup->_lastSeen.end()) or ((now - it->second) > 0.25f);
              dedup->_lastSeen[other] = now;
              if (enter) {
                if (_pysys) // resolved once in _onLink
                  _pysys->_notify("Collision"_tok, data); // compile-time token, PUBLIC notify entry
              }
            }
          };
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
        if (rigid_body->isStaticObject() and not CDATA._isKinematic) {
          // static world geometry must read INACTIVE: an active static partner
          // forces narrowphase on every pair it touches each substep (and
          // WANTS_DEACTIVATION never transitions for statics — they would stay
          // active forever). Sleeping is the correct steady state.
          rigid_body->forceActivationState(ISLAND_SLEEPING);
        } else {
          rigid_body->setActivationState(ballowsleep ? WANTS_DEACTIVATION : DISABLE_DEACTIVATION);
          rigid_body->activate();
        }
        if (DEBUG_LOG) {
          logchan_bull->log("BulletObjectComponent<%p> rigid_body<%p>", (void*)component, (void*)rigid_body);
        }
        component->_rigidbody = rigid_body;

        if (component->mBOCD._angularFactor.magnitude() > 0.1f) {
          rigid_body->setAngularFactor(orkv3tobtv3(component->mBOCD._angularFactor));
        }

        // launch velocity — the ONE per-spawn velocity channel: the entity
        // varmap's "initialVelocity", written by BOTH the FSM's scheduled
        // spawns (InitialSpeed/InitialDirection/DirectionRandomize) and the
        // script Spawner.spawn(vel=...) path. This consume is what makes the
        // long-written-never-read FSM channel live.
        if (auto iv = entity->_varmap->typedValueForKey<fvec3>("initialVelocity"))
          rigid_body->setLinearVelocity(orkv3tobtv3(iv.value()));
        // angular twin (Spawner.spawn(avel=...)): rad/s about a world axis —
        // projectile topspin that converts to forward drive on a frictional
        // contact. Respects the angularFactor lock set above when present.
        if (auto iav = entity->_varmap->typedValueForKey<fvec3>("initialAngularVelocity"))
          rigid_body->setAngularVelocity(orkv3tobtv3(iav.value()));
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

  // instance pairing — wire the motion state to the SG component's instance
  // slot. For DYNAMIC spawns the SG stage render-op runs after this activate,
  // so the callback fires later; for STATIC (autospawned) entities the render
  // op may already have drained between the stage and activate phases — in
  // that case _INSTANCE already exists and we wire IMMEDIATELY (the callback
  // would never fire; this was the silent never-wired hole).
  if (component->_mySGcomponentForInstancing) {
    auto sgcomp = component->_mySGcomponentForInstancing;
    auto wire   = [=]() {
      auto instance = sgcomp->_INSTANCE;
      OrkAssert(instance);
      component->_sginstance_id = instance->_instance_index;
      OrkAssert(component->_sginstance_id >= 0);
      auto idata                   = instance->_idata;
      auto entmotionstate          = (EntMotionState*)component->_rigidbody->getMotionState();
      entmotionstate->_instance_id = component->_sginstance_id;
      entmotionstate->_idata       = idata;
    };
    if (sgcomp->_INSTANCE)
      wire();
    else
      sgcomp->_onInstanceCreated = wire;
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
    // batch-expanded shapes (e.g. scatter proxies): the factory parked its
    // per-item static bodies on the shape inst — remove them with the component.
    if (auto shapeinst = component->_shapeinst) {
      if (auto try_batch = shapeinst->_impl.tryAs<shapebatch_ptr_t>()) {
        auto batch = try_batch.value();
        for (auto body : batch->_bodies) {
          world->removeRigidBody(body);
          delete body->getMotionState();
          delete body;
        }
        for (auto shape : batch->_ownedShapes)
          delete shape;
        batch->_bodies.clear();
        batch->_ownedShapes.clear();
      }
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

void OrkContactResultCallback::emitContact(
    const btManifoldPoint& cp,
    const btCollisionObject* colObj0,
    const btCollisionObject* colObj1) {

  if (not _onContact)
    return;

  btRigidBody* body0 = (btRigidBody*)colObj0;
  btRigidBody* body1 = (btRigidBody*)colObj1;

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
  datatable["entityA"_tok]   = pyentity_ptr_t(entA);
  datatable["entityB"_tok]   = pyentity_ptr_t(entB);
  datatable["entrefA"_tok]   = erefA;
  datatable["entrefB"_tok]   = erefB;
  datatable["groupA"_tok]    = group0;
  datatable["groupB"_tok]    = group1;
  datatable["pointA"_tok]    = btv3toorkv3(ptA);
  datatable["pointB"_tok]    = btv3toorkv3(ptB);
  datatable["normalOnB"_tok] = btv3toorkv3(normalOnB).normalized();
  // E.2-walk: plain entity NAMES for script-side interception (the python wrappers
  // above are host-specific; names are universal).
  datatable["nameA"_tok]     = std::string(entA->name().c_str());
  datatable["nameB"_tok]     = std::string(entB->name().c_str());

  auto sim = _system->simulation();
  sim->_enqueueDeferredInvokation(invocation);
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
    emitContact(cp, colObj0Wrap->getCollisionObject(), colObj1Wrap->getCollisionObject());
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
  // bullet DEFAULTS to recomputing EVERY object's AABB each substep
  // (m_forceUpdateAllAabbs=true) — with thousands of static scatter bodies that
  // is O(objects) dbvt churn per substep (measured: step=1000ms/s at 8.4k
  // statics, scaling with density). Off = only ACTIVE objects refresh their
  // AABBs, which is the entire point of static+sleeping bodies. Anything that
  // teleports a body while it is asleep must activate() it (or call
  // updateSingleAabb) so the broadphase sees the move.
  mDynamicsWorld->setForceUpdateAllAabbs(false);
  // never CREATE static<->static broadphase pairs. A world-spanning AABB (the
  // terrain heightfield) overlaps every static scatter body — without this
  // filter that is one pair PER ROCK, each narrowphase-dispatched per substep
  // whenever either partner reads as active (measured: step=1000ms/s at 8.4k
  // statics). Bullet can never produce a useful result from a static-static
  // pair; reject them before the pair cache.
  struct StaticPairFilter : public btOverlapFilterCallback {
    bool needBroadphaseCollision(btBroadphaseProxy* p0, btBroadphaseProxy* p1) const final {
      bool collides = (p0->m_collisionFilterGroup & p1->m_collisionFilterMask) //
                  and (p1->m_collisionFilterGroup & p0->m_collisionFilterMask);
      if (collides) {
        auto o0 = (btCollisionObject*)p0->m_clientObject;
        auto o1 = (btCollisionObject*)p1->m_clientObject;
        if (o0->isStaticObject() and o1->isStaticObject())
          collides = false;
      }
      return collides;
    }
  };
  _overlapFilter = new StaticPairFilter;
  mDynamicsWorld->getPairCache()->setOverlapFilterCallback(_overlapFilter);
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
  drw->_sortkey = 0x3fffffff;

  auto pdata       = new BulletDebugDrawDBData(_debugger);
  pdata->_debugger = _debugger;
  _debugDrawable->SetUserDataA(pdata);
  _debugDrawable->_name = "BulletSystemDebugger";

  _sgsystem = psi->findSystem<SceneGraphSystem>();
  OrkAssert(_sgsystem != nullptr);

  _sgsystem->_addStaticDrawable("std_forward", _debugDrawable);

  // resolved ONCE — the collision-notify path forwards to it at event rate
  // (null when the scene declares no PythonSystem).
  _pysys = psi->_findSystemFromName("PythonSystem");

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
  OrkProfilerSampleScope(CHANNEL_UPDATE, "BulletSystem::_onUpdate");
  if (mDynamicsWorld) {
    // drain deferred terrain hot-reloads BEFORE stepSimulation reads the heightfield: a rebake
    // rewrote the artifacts in place and flagged the collider (possibly from the GPU thread); the
    // actual _heightData mutation happens here, on the update thread, where nothing is stepping.
    for (auto& item : _terrainReloadPolls)
      item.second();

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

      {
        OrkProfilerSampleScope(CHANNEL_UPDATE, "bullet::kinematic");
        for (BulletObjectComponent* component : _updateKinematicComponents._linear) {
          component->updateKinematic(_simulation, dt);
        }
      }
      {
        OrkProfilerSampleScope(CHANNEL_UPDATE, "bullet::dynamic");
        for (BulletObjectComponent* component : _updateDynamicComponents._linear) {
          component->updateDynamic(_simulation, dt);
        }
      }
      {
        OrkProfilerSampleScope(CHANNEL_UPDATE, "bullet::forces");
        for (BulletObjectComponent* component : _updateForceComponents._linear) {
          component->updateForces(_simulation, dt);
        }
      }

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
          sgcomp->_onNotify(inst, "ChangeModColor"_crcu, fvec4(0.5, 0.5, 0.5, 1));
          // mDynamicsWorld->removeRigidBody(rbody);
        }
        /////////////////////////////////////////
      }

      // diagnostic wall-clock telemetry (the profiler scopes compile out in
      // release builds): ms spent in step vs collision-scan, printed every 2s.
      static double _diag_step_ms = 0.0, _diag_scan_ms = 0.0;
      static double _diag_last_print = 0.0;
      auto _diag_now = [] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
      };

      double _diag_t0 = _diag_now();
      {
        OrkProfilerSampleScope(CHANNEL_UPDATE, "bullet::stepSimulation");
        int a = mDynamicsWorld->stepSimulation(fdts, mMaxSubSteps, ffts);
        int b = mMaxSubSteps;
        int m = std::min(a, b); // ? a : b; // ork::min()
        mNumSubStepsTaken += m;
      }
      double _diag_t1 = _diag_now();
      _diag_step_ms += (_diag_t1 - _diag_t0) * 1000.0;

      {
        OrkProfilerSampleScope(CHANNEL_UPDATE, "bullet::collisions");
        // MANIFOLD SCAN, not contactTest. stepSimulation already produced the
        // persistent contact manifolds (broadphase-culled, incrementally updated);
        // reading them is O(actual contacts). contactTest re-runs narrowphase per
        // call — and against a btCompoundShape it constructs a collision algorithm
        // that PREALLOCATES one child algorithm PER COMPOUND CHILD, making the
        // per-tick cost proportional to scatter density (the observed 1-FPS
        // slowdown at ~4k rocks). Never query what the step already computed.
        if (not _collisionCallbacks.empty()) {
          auto dispatcher = mDynamicsWorld->getDispatcher();
          const int nman  = dispatcher->getNumManifolds();
          for (int i = 0; i < nman; i++) {
            auto manifold  = dispatcher->getManifoldByIndexInternal(i);
            const int ncon = manifold->getNumContacts();
            if (0 == ncon)
              continue;
            auto body0 = manifold->getBody0();
            auto body1 = manifold->getBody1();
            for (auto callback : _collisionCallbacks) {
              auto mon = callback->monitoredBody;
              if (body0 == mon or body1 == mon) {
                // deepest point only — gameplay wants THE contact, not the patch
                int best = 0;
                for (int c = 1; c < ncon; c++)
                  if (manifold->getContactPoint(c).getDistance() < manifold->getContactPoint(best).getDistance())
                    best = c;
                callback->emitContact(manifold->getContactPoint(best), body0, body1);
              }
            }
          }
        }
      }
      double _diag_t2 = _diag_now();
      _diag_scan_ms += (_diag_t2 - _diag_t1) * 1000.0;
      if ((_diag_t2 - _diag_last_print) > 2.0) {
        if (_diag_last_print != 0.0) {
          const double span = _diag_t2 - _diag_last_print;
          if(0)printf(
              "bullet timing: step=%.2fms/s scan=%.3fms/s objects=%d\n",
              _diag_step_ms / span,
              _diag_scan_ms / span,
              mDynamicsWorld->getNumCollisionObjects());
        }
        _diag_step_ms    = 0.0;
        _diag_scan_ms    = 0.0;
        _diag_last_print = _diag_t2;
      }
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
