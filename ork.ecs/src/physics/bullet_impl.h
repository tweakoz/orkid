#pragma once  

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder-ctor"

#include <LinearMath/btIDebugDraw.h>
#include <btBulletDynamicsCommon.h>

#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btStaticPlaneShape.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <BulletCollision/Gimpact/btGImpactShape.h>
//#include <Extras/GIMPACTUtils/btGImpactConvexDecompositionShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>

#pragma GCC diagnostic pop

#include <ork/kernel/mutex.h>
#include <ork/kernel/orkpool.inl>
#include <ork/kernel/concurrent_queue.h>

#include <ork/ecs/physics/bullet.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/util/fast_set.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

ork::fvec3 btv3toorkv3(const btVector3& v3);
ork::fquat btqtoorkq(const btQuaternion& q);
ork::fmtx4 btmtx4toorkmtx4(const btTransform& mtx);
ork::fmtx3 btbasistoorkmtx3(const btMatrix3x3& mtx);

btVector3 orkv3tobtv3(const fvec3& v3);
btTransform orkmtx4tobtmtx4(const fmtx4& mtx);
btQuaternion orkqtobtq(const fquat& q);
btMatrix3x3 orkmtx3tobtbasis(const fmtx3& mtx);

///////////////////////////////////////////////////////////////////////////////

btBoxShape* meshToBoxShape(meshutil::flatsubmesh_ptr_t mesh, float fscale);
btSphereShape* meshToSphereShape(meshutil::flatsubmesh_ptr_t mesh, float fscale);
//btCompoundShape* meshToCompoundShape(lev2::XgmMesh* xgmmesh, float fscale);
//btCollisionShape* ClusterToBvhTriangleMeshShape(lev2::xgmcluster_ptr_t xgmcluster, float fscale);
btCollisionShape* meshToGimpactCompoundShape(meshutil::flatsubmesh_ptr_t mesh, float fscale);

///////////////////////////////////////////////////////////////////////////////

struct BulletDebugDrawDBRec {};

///////////////////////////////////////////////////////////////////////////////

struct BulletDebugDrawDBData {
  std::vector<BulletDebugDrawDBRec> _DBRecs;
  PhysicsDebugger* _debugger;
  BulletDebugDrawDBData(PhysicsDebugger* debugger);
  ~BulletDebugDrawDBData();
};

///////////////////////////////////////////////////////////////////////////////

struct PhysicsDebuggerLine {
  fvec3 mFrom;
  fvec3 mTo;
  fvec3 mColor;

  PhysicsDebuggerLine(const fvec3& f, const fvec3& t, const fvec3& c) : mFrom(f), mTo(t), mColor(c) {}
};

///////////////////////////////////////////////////////////////////////////////

struct PhysicsDebugger final : public btIDebugDraw {

  typedef std::vector<PhysicsDebuggerLine> lineq_t;
  typedef lineq_t* lineqptr_t;

  PhysicsDebugger();

  void _onGpuInit(Simulation* psi, lev2::Context* ctx);
  void _onGpuExit(Simulation* psi, lev2::Context* ctx);

  void addLine(const ork::fvec3& from, const ork::fvec3& to, const ork::fvec3& color);
  void render(const ork::lev2::RenderContextInstData& rcid, lineqptr_t lines);
  void SetDebug(bool bv) { _enabled = bv; }

  //////////////////////////

  void beginSimFrame(BulletSystem*system);
  void endSimFrame(BulletSystem*system);
  void beginRenderFrame();
  void endRenderFrame();

  //////////////////////////

  void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) final;
  void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime,
                        const btVector3& color) final;
virtual void  drawSphere (btScalar radius, const btTransform &transform, const btVector3 &color) final;


  void reportErrorWarning(const char* warningString) final;
  void draw3dText(const btVector3& location, const char* textString) final;
  void setDebugMode(int debugMode) final;
  int getDebugMode() const override;

  MpMcBoundedQueue<lineqptr_t,4> _lineqpool;
  lineqptr_t _currentwritelq = nullptr;
  std::atomic<lineqptr_t> _curreadlq;
  lineqptr_t _checkedoutreadlq = nullptr;
  bool _enabled = false;

  // emission bounds (self-defend): heightfield colliders debug-draw EVERY triangle —
  // a 1024^2 terrain emits ~12M lines/frame, far past the shared dynamic VB (1M verts).
  // Lines are radius-filtered around the camera eye (the collider-vs-visual gauge is
  // local by nature) then hard-capped; both bounds warn loudly when they clip.
  fvec3 _refPoint;
  bool _hasRefPoint  = false;
  float _refRadius   = 100.0f; // meters; ORKID_PHYSDBG_RADIUS_M overrides
  size_t _linesDropped = 0;    // per sim frame, cap overflow only
  static constexpr size_t kMaxLines = 200000; // 400k verts per eye pass; 800k/frame dual-mono VR (< the 1M shared ring)

  static constexpr int kmaxbuffers =20;
    //mDBRecs[i]; //._bulletSystem = system
  std::vector<BulletDebugDrawDBData> mDBRecs;
  std::shared_ptr<lev2::FreestyleMaterial> _material;
  const lev2::FxShaderTechnique* _technique = nullptr;
  const lev2::FxShaderParam* _paramMVP = nullptr;
  const lev2::FxShaderParam* _paramModColor = nullptr;


  //////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class EntMotionState : public btMotionState {
public:
  EntMotionState(const btTransform& initialpos, Entity* entity);

  virtual void getWorldTransform(btTransform& transform) const;
  virtual void setWorldTransform(const btTransform& transform);

  Entity* mEntity;
  btTransform mTransform;
  dvec3 _energy;
  dvec3 _prevpos;
  int _counter = 0;
  bool _permadeactived = false;

  int _instance_id = -1;
  lev2::instanceddrawinstancedata_ptr_t _idata;

};

///////////////////////////////////////////////////////////////////////////////

struct BulletObjectForceControllerInst {
public:
  BulletObjectForceControllerInst();
  virtual ~BulletObjectForceControllerInst();
  virtual void UpdateForces(BulletObjectComponent* boci, float deltat) = 0;
  virtual bool DoLink(Simulation* psi) = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletShapeBaseInst {
  BulletShapeBaseInst(const BulletShapeBaseData* data);

  const AABox& GetBoundingBox() const { return mBoundingBox; }

  const BulletShapeBaseData* _shapeData = nullptr;
  btCollisionShape* _collisionShape = nullptr;
  AABox mBoundingBox;
  lev2::callback_drawable_ptr_t _drawable;
  svar16_t _impl;
  // W·M SURFACE RESPONSE (terrain physics leg): the terrain shape raises this when per-contact
  // friction modulation is active — the system then flags the rigid body CF_CUSTOM_MATERIAL_CALLBACK
  // so bullet routes its contacts through the global contact-added hook (installed shape-side).
  // Generic bool so the base does not depend on the terrain impl.
  bool _wantsCustomMaterialCallback = false;
};

// a shape factory may expand into MANY broadphase entries instead of one compound
// (per-item static bodies sharing per-type base shapes — bullet's many-instances
// idiom: small per-body AABBs let broadphase cull, where a single scene-spanning
// compound forces midphase work onto every query). The factory parks them here
// (inst->_impl); the system removes + deletes them at component deactivate.
struct ShapeBatchBodies {
  std::vector<btRigidBody*> _bodies;
  std::vector<btCollisionShape*> _ownedShapes; // per-item wrappers + shared bases
};
using shapebatch_ptr_t = std::shared_ptr<ShapeBatchBodies>;

///////////////////////////////////////////////////////////////////////////////

class OrkContactResultCallback : public btCollisionWorld::ContactResultCallback {
public:
    BulletSystem* _system;
    btRigidBody* monitoredBody;
    script_cb_t _onContact;

    OrkContactResultCallback(btRigidBody* body);

    // build + enqueue the contact payload for a manifold point. Called from the
    // per-update MANIFOLD SCAN (stepSimulation's persistent manifolds — O(actual
    // contacts), independent of shape complexity). contactTest is NOT used for
    // monitoring: against a btCompoundShape it constructs a collision algorithm
    // per call, which PREALLOCATES a child algorithm PER CHILD — per-tick cost
    // proportional to scatter density (the observed slowdown).
    void emitContact(const btManifoldPoint& cp, const btCollisionObject* obj0, const btCollisionObject* obj1);

    btScalar addSingleResult(btManifoldPoint& cp,
                             const btCollisionObjectWrapper* colObj0Wrap,
                             int partId0,
                             int index0,
                             const btCollisionObjectWrapper* colObj1Wrap,
                             int partId1,
                             int index1) final;
};

using orkcontactcallback_ptr_t = std::shared_ptr<OrkContactResultCallback>;

///////////////////////////////////////////////////////////////////////////////

struct BulletObjectComponent : public Component {
  DeclareAbstractX(BulletObjectComponent, Component);

public:
  BulletObjectComponent(const BulletObjectComponentData& data, Entity* entity);
  ~BulletObjectComponent();

  const BulletObjectComponentData& data() const { return mBOCD; }

  BulletObjectForceControllerInst* getForceController(std::string named) const;

  void updateDynamic(Simulation* sim, float time_step);
  void updateForces(Simulation* sim, float time_step);
  void updateKinematic(Simulation* sim, float time_step);

  const BulletObjectComponentData& mBOCD;
  orkmap<std::string, BulletObjectForceControllerInst*> _forces;

  btRigidBody* _rigidbody = nullptr;
  BulletShapeBaseInst* _shapeinst = nullptr;
  orkcontactcallback_ptr_t _collisionCallback;
  SceneGraphComponent* _mySGcomponentForInstancing = nullptr;
  float _birthtime = 0.0;
  int _sginstance_id = -1;
  void _onNotify(Simulation* psi, token_t evID, evdata_t data ) final;
  void _onRequest(Simulation* psi, impl::comp_response_ptr_t response, token_t evID, evdata_t data) final;

  bool _onLink(Simulation* sim) final;
  void _onUnlink(Simulation* sim) final;
  bool _onStage(Simulation* sim) final;
  void _onUnstage(Simulation* sim) final;
  bool _onActivate(Simulation* sim) final;
  void _onDeactivate(Simulation* sim) final;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletSystem : public System {
  DeclareAbstractX(BulletSystem, System);

public:
  static constexpr systemkey_t SystemType = "BulletSystem";
  systemkey_t systemTypeDynamic() final { return SystemType; }

  BulletSystem(const BulletSystemData& data, Simulation* psi);
  ~BulletSystem();

  btRigidBody* AddLocalRigidBody(Entity* pent, btScalar mass, const btTransform& startTransform, btCollisionShape* shape, uint32_t group_assign,uint32_t groups_collides_with);
  btDiscreteDynamicsWorld* GetDynamicsWorld() const { return mDynamicsWorld; }
  void InitWorld();
  PhysicsDebugger* Debugger() { return _debugger; }
  btDynamicsWorld* BulletWorld();
  int GetMaxSubSteps() const { return mMaxSubSteps; }
  void SetMaxSubSteps(int maxsubsteps) { mMaxSubSteps = maxsubsteps; }
  int GetNumSubStepsTaken() const { return mNumSubStepsTaken; }
  const BulletSystemData& GetWorldData() const { return _systemData; }

  void _onLinkComponent(BulletObjectComponent* component);
  void _onUnlinkComponent(BulletObjectComponent* component);
  void _onStageComponent(BulletObjectComponent* component);
  void _onUnstageComponent(BulletObjectComponent* component);
  void _onActivateComponent(BulletObjectComponent* component);
  void _onDeactivateComponent(BulletObjectComponent* component);

  void _onUpdate(Simulation* inst) final;
  bool _onLink(Simulation* psi) final;
  //void enqueueDrawables(lev2::DrawQueue& buffer) final;
  void _onGpuInit(Simulation* psi, lev2::Context* ctx) final;
  void _onGpuLink(Simulation* psi, lev2::Context* ctx) final;
  void _onGpuExit(Simulation* psi, lev2::Context* ctx) final;

  void _onBeginRender() final;
  void _onEndRender() final;

  void _onNotify(token_t evID, evdata_t data) final;

  //void beginRenderFrame(const Simulation* psi) final;
  //void endRenderFrame(const Simulation* psi) final;

  lev2::drawable_ptr_t _debugDrawable = nullptr;
  btDiscreteDynamicsWorld* mDynamicsWorld;
  btDefaultCollisionConfiguration* mBtConfig;
  btBroadphaseInterface* mBroadPhase;
  btCollisionDispatcher* mDispatcher;
  btSequentialImpulseConstraintSolver* mSolver;
  btOverlapFilterCallback* _overlapFilter = nullptr;
  const BulletSystemData& _systemData;
  std::string _dbgdrawlayername;
  lev2::DrawQueueTransferData _dbgdrawXF;
  PhysicsDebugger* _debugger = nullptr;
  bool _debugToggle = false; // runtime TOGGLE_DEBUG_DRAW state, XORed with the reflected Debug prop
  int mMaxSubSteps;
  System* _pysys = nullptr; // the scene's PythonSystem, resolved once in _onLink (may be null)
  int mNumSubStepsTaken;
  float mfAvgDtAcc;
  float mfAvgDtCtr;
  SceneGraphSystem* _sgsystem = nullptr;
  float _fdtaccum = 0.0f;
  tsl::robin_pg_map<const BulletObjectComponentData*,BulletObjectComponent*> _lastcomponentfordata;
  tsl::robin_pg_set<BulletObjectComponent*> _activeComponents;
  fast_set<BulletObjectComponent*> _updateForceComponents;
  fast_set<BulletObjectComponent*> _updateKinematicComponents;
  fast_set<BulletObjectComponent*> _updateDynamicComponents;
  fast_set<BulletObjectComponent*> _updateCheckComponents;
  fast_set<BulletObjectComponent*> _sleptDynamicComponents;
  tsl::robin_set<orkcontactcallback_ptr_t> _collisionCallbacks;
  tsl::robin_pg_set<BulletObjectComponent*> _deactivation_queue;
  // deferred terrain hot-reload polls (keyed by the terrain impl pointer). A same-path terrain
  // rebake flags the impl (msgrouter, possibly on the GPU thread); _onUpdate drains these on the
  // update thread — the safe point to mutate heightfield data the sim steps read.
  std::map<const void*, std::function<void()>> _terrainReloadPolls;
  // SELF-DEFENSE terrain floor: terrain colliders report their world-Y surface span here at
  // shape creation so the character controller can NAME an underground spawn (spawn.y below the
  // terrain minimum ⇒ ground contact impossible) and recover the fall to walkable ground. The
  // span is the UNION across all registered terrain colliders (entity at y=0 == baked meters).
  void _registerTerrainYSpan(float minY, float maxY);
  bool  _hasTerrainFloor = false;
  float _terrainMinY     = 0.0f;
  float _terrainMaxY     = 0.0f;
};


class DirectionalForceInst final : public BulletObjectForceControllerInst {
public:
  DirectionalForceInst(const DirectionalForceData* data = nullptr);
  ~DirectionalForceInst();
  void UpdateForces(BulletObjectComponent* boci, float deltat) final;
  bool DoLink(Simulation* psi) final;

  const DirectionalForceData* _DFD;

};

} //namespace ork::ecs {
