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
#include <ork/kernel/orkpool.h>
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

  static constexpr int kmaxbuffers =20;
    //mDBRecs[i]; //._bulletSystem = system
  std::vector<BulletDebugDrawDBData> mDBRecs;
  lev2::pbrmaterial_ptr_t _pbrmaterial;
  lev2::fxpipelinecache_constptr_t _fxcache;


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
};

///////////////////////////////////////////////////////////////////////////////

class OrkContactResultCallback : public btCollisionWorld::ContactResultCallback {
public:
    BulletSystem* _system;
    btRigidBody* monitoredBody;
    script_cb_t _onContact;

    OrkContactResultCallback(btRigidBody* body);

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
  //void enqueueDrawables(lev2::DrawableBuffer& buffer) final;
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
  const BulletSystemData& _systemData;
  std::string _dbgdrawlayername;
  lev2::DrawQueueXfData _dbgdrawXF;
  PhysicsDebugger* _debugger = nullptr;
  int mMaxSubSteps;
  int mNumSubStepsTaken;
  float mfAvgDtAcc;
  float mfAvgDtCtr;
  SceneGraphSystem* _sgsystem = nullptr;
  float _fdtaccum = 0.0f;
  std::unordered_map<const BulletObjectComponentData*,BulletObjectComponent*> _lastcomponentfordata;
  std::unordered_set<BulletObjectComponent*> _activeComponents;
  fast_set<BulletObjectComponent*> _updateForceComponents;
  fast_set<BulletObjectComponent*> _updateKinematicComponents;
  fast_set<BulletObjectComponent*> _updateDynamicComponents;
  fast_set<BulletObjectComponent*> _updateCheckComponents;
  fast_set<BulletObjectComponent*> _sleptDynamicComponents;
  std::unordered_set<orkcontactcallback_ptr_t> _collisionCallbacks;
  std::unordered_set<BulletObjectComponent*> _deactivation_queue;
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
