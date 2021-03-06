////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "componentfamily.h"
#include <ork/kernel/mutex.h>
#include <ork/kernel/orkpool.h>
#include <ork/math/PIDController.h>
#include <ork/math/box.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/object/Object.h>
#include <pkg/ent/component.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/terrain/heightmap.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/kernel/concurrent_queue.h>

//#define BT_USE_DOUBLE_PRECISION

#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <LinearMath/btIDebugDraw.h>
#include <btBulletDynamicsCommon.h>

class btDynamicsWorld;
class btCollisionShape;
class btRigidBody;
class btBvhTriangleMeshShape;
class btCompoundShape;
class btBoxShape;
class btSphereShape;
class btDefaultCollisionConfiguration;
class btVector3;

namespace ork::lev2 {
class XgmModel;
class XgmMesh;
class XgmCluster;
class Context;
class RenderContextInstData;
} // namespace ork::lev2

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

class BulletObjectForceControllerInst;
class BulletObjectControllerInst;
class BulletObjectControllerData;
class BulletShapeBaseInst;
class BulletShapeBaseData;
class BulletObjectControllerData;
class BulletSystem;

///////////////////////////////////////////////////////////////////////////////

btVector3 orkv3tobtv3(const ork::fvec3& v3);
ork::fvec3 btv3toorkv3(const btVector3& v3);
btTransform orkmtx4tobtmtx4(const ork::fmtx4& mtx);
btMatrix3x3 orkmtx3tobtbasis(const ork::fmtx3& mtx);
ork::fmtx4 btmtx4toorkmtx4(const btTransform& mtx);
ork::fmtx3 btbasistoorkmtx3(const btMatrix3x3& mtx);

///////////////////////////////////////////////////////////////////////////////

inline const btVector3& operator<<=(btVector3& lhs, const ork::fvec3& v3) {
  lhs = orkv3tobtv3(v3);
  return lhs;
}
inline btVector3 operator!(const ork::fvec3& v3) { return orkv3tobtv3(v3); }
inline ork::fvec3 operator!(const btVector3& v3) { return btv3toorkv3(v3); }

inline btTransform operator!(const ork::fmtx4& mtx) { return orkmtx4tobtmtx4(mtx); }
inline ork::fmtx4 operator!(const btTransform& mtx) { return btmtx4toorkmtx4(mtx); }

inline btMatrix3x3 operator!(const ork::fmtx3& mtx) { return orkmtx3tobtbasis(mtx); }
inline ork::fmtx3 operator!(const btMatrix3x3& mtx) { return btbasistoorkmtx3(mtx); }

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
  void reportErrorWarning(const char* warningString) final;
  void draw3dText(const btVector3& location, const char* textString) final;
  void setDebugMode(int debugMode) final;
  int getDebugMode() const override;

  MpMcBoundedQueue<lineqptr_t,4> _lineqpool;
  lineqptr_t _currentwritelq = nullptr;
  std::atomic<lineqptr_t> _curreadlq;
  lineqptr_t _checkedoutreadlq = nullptr;
  bool _enabled = false;

  //////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct BulletDebugDrawDBRec {};

///////////////////////////////////////////////////////////////////////////////

struct BulletDebugDrawDBData {
  BulletSystem* _bulletSystem;
  BulletDebugDrawDBRec mDBRecs[lev2::DrawableBuffer::kmaxbuffers];
  PhysicsDebugger* _debugger;
  BulletDebugDrawDBData(BulletSystem* psi);
  ~BulletDebugDrawDBData();
};

///////////////////////////////////////////////////////////////////////////////

class BulletSystemData : public SystemData {
  RttiDeclareConcrete(BulletSystemData, SystemData);

  float mfTimeScale;
  float mSimulationRate;
  bool mbDEBUG;
  fvec3 mGravity;

public:
  BulletSystemData();

  float GetTimeScale() const { return mfTimeScale; }
  bool IsDebug() const { return mbDEBUG; }
  float GetSimulationRate() const { return mSimulationRate; }
  const fvec3& GetGravity() const { return mGravity; }

protected:
  System* createSystem(Simulation* psi) const final;
};

///////////////////////////////////////////////////////////////////////////////

class BulletSystem : public ork::ent::System {

  void DoUpdate(ork::ent::Simulation* inst) final;

public:
  static constexpr systemkey_t SystemType = "BulletSystem";
  systemkey_t systemTypeDynamic() final { return SystemType; }

  BulletSystem(const BulletSystemData& data, ork::ent::Simulation* psi);
  ~BulletSystem();

  btRigidBody* AddLocalRigidBody(ork::ent::Entity* pent, btScalar mass, const btTransform& startTransform, btCollisionShape* shape);

  btDiscreteDynamicsWorld* GetDynamicsWorld() const { return mDynamicsWorld; }

  bool DoLink(Simulation* psi) final;
  void LinkPhysicsObject(ork::ent::Simulation* inst, ork::ent::Entity* entity);
  void enqueueDrawables(lev2::DrawableBuffer& buffer) final;

  void InitWorld();

  PhysicsDebugger& Debugger() { return _debugger; }
  btDynamicsWorld* BulletWorld() { return mDynamicsWorld; }

  int GetMaxSubSteps() const { return mMaxSubSteps; }
  void SetMaxSubSteps(int maxsubsteps) { mMaxSubSteps = maxsubsteps; }

  int GetNumSubStepsTaken() const { return mNumSubStepsTaken; }

  const BulletSystemData& GetWorldData() const { return _systemData; }
  void beginRenderFrame(const Simulation* psi) final;
  void endRenderFrame(const Simulation* psi) final;

private:

  lev2::CallbackDrawable* _debugDrawable = nullptr;
  btDiscreteDynamicsWorld* mDynamicsWorld;
  btDefaultCollisionConfiguration* mBtConfig;
  btBroadphaseInterface* mBroadPhase;
  btCollisionDispatcher* mDispatcher;
  btSequentialImpulseConstraintSolver* mSolver;
  const BulletSystemData& _systemData;
  std::string _dbgdrawlayername;
  lev2::DrawQueueXfData _dbgdrawXF;
  PhysicsDebugger _debugger;
  int mMaxSubSteps;
  int mNumSubStepsTaken;
  float mfAvgDtAcc;
  float mfAvgDtCtr;
};

///////////////////////////////////////////////////////////////////////////////

class EntMotionState : public btMotionState {
public:
  EntMotionState(const btTransform& initialpos, ork::ent::Entity* entity);

  virtual void getWorldTransform(btTransform& transform) const;

  virtual void setWorldTransform(const btTransform& transform);

protected:
  ork::ent::Entity* mEntity;
  btTransform mTransform;
};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectArchetype : public ork::ent::Archetype {
  RttiDeclareConcrete(BulletObjectArchetype, ork::ent::Archetype);

private:
  void DoCompose(ork::ent::ArchComposer& composer) override;
  void DoLinkEntity(ork::ent::Simulation* inst, ork::ent::Entity* pent) const override;
  void DoStartEntity(ork::ent::Simulation* inst, const ork::fmtx4& world, ork::ent::Entity* pent) const override;
};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectForceControllerData : public ork::Object {
  RttiDeclareAbstract(BulletObjectForceControllerData, ork::Object);

public:
  BulletObjectForceControllerData();
  ~BulletObjectForceControllerData();

  virtual BulletObjectForceControllerInst* CreateForceControllerInst(const BulletObjectControllerData& data,
                                                                     ork::ent::Entity* pent) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectForceControllerInst {
public:
  BulletObjectForceControllerInst(const BulletObjectForceControllerData& data);
  virtual ~BulletObjectForceControllerInst();
  virtual void UpdateForces(ork::ent::Simulation* inst, BulletObjectControllerInst* boci) = 0;
  virtual bool DoLink(ent::Simulation* psi) = 0;

private:
  const BulletObjectForceControllerData& mData;
};

///////////////////////////////////////////////////////////////////////////////

struct ShapeCreateData {
  Entity* mEntity;
  BulletSystem* mWorld;
  BulletObjectControllerInst* mObject;
};

struct ShapeFactory {

  typedef std::function<BulletShapeBaseInst*(const ShapeCreateData& data)> creator_t;
  typedef std::function<void(BulletShapeBaseData*)> invalidator_t;

  ShapeFactory(creator_t c = nullptr, invalidator_t i = [](BulletShapeBaseData*) {})
      : _createShape(c), _invalidate(i), _impl(nullptr) {}

  creator_t _createShape;
  invalidator_t _invalidate;
  svarp_t _impl;
};

typedef ShapeFactory shape_factory_t;

class BulletShapeBaseData : public ork::Object {
  RttiDeclareAbstract(BulletShapeBaseData, ork::Object);

public:
  BulletShapeBaseData();
  ~BulletShapeBaseData();

  BulletShapeBaseInst* CreateShape(const ShapeCreateData& data) const;

protected:
  shape_factory_t mShapeFactory;

  void doNotify(const event::Event* event) override;
};

struct BulletShapeBaseInst {
  BulletShapeBaseInst(const BulletShapeBaseData* data = nullptr) : _shapeData(data), mCollisionShape(nullptr), _impl(nullptr) {}

  const AABox& GetBoundingBox() const { return mBoundingBox; }
  btCollisionShape* GetBulletShape() const { return mCollisionShape; }

  const BulletShapeBaseData* _shapeData;
  btCollisionShape* mCollisionShape;
  AABox mBoundingBox;
  lev2::callback_drawable_ptr_t _drawable;
  svar16_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeCapsuleData : public BulletShapeBaseData {
  RttiDeclareConcrete(BulletShapeCapsuleData, BulletShapeBaseData);

public:
  BulletShapeCapsuleData();

private:
  float mfRadius;
  float mfExtent;
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapePlaneData : public BulletShapeBaseData {
  RttiDeclareConcrete(BulletShapePlaneData, BulletShapeBaseData);

public:
  BulletShapePlaneData();
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeSphereData : public BulletShapeBaseData {
  RttiDeclareConcrete(BulletShapeSphereData, BulletShapeBaseData);

public:
  BulletShapeSphereData();

private:
  float mfRadius;
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeModelData : public BulletShapeBaseData {
  RttiDeclareConcrete(BulletShapeModelData, BulletShapeBaseData);

public:
  BulletShapeModelData();
  ~BulletShapeModelData();

  lev2::XgmModelAsset* asset() { return mModelAsset; }
  void SetModelAccessor(ork::rtti::ICastable* const& mdl);
  void GetModelAccessor(ork::rtti::ICastable*& mdl) const;
  float GetScale() const { return mfScale; }

private:
  lev2::XgmModelAsset* mModelAsset;
  float mfScale;
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeTerrainData : public BulletShapeBaseData {
  RttiDeclareConcrete(BulletShapeTerrainData, BulletShapeBaseData);

public:
  BulletShapeTerrainData();
  ~BulletShapeTerrainData();

  void SetHeightMapName(file::Path const& lmap);
  void GetHeightMapName(file::Path& lmap) const;

  const file::Path& HeightMapPath() const { return mHeightMapName; }
  float WorldHeight() const { return mWorldHeight; }
  float WorldSize() const { return mWorldSize; }

private:
	ork::Object* _visualDataAccessor() { return & _visualData; }

  bool postDeserialize(reflect::serdes::IDeserializer&) final;

  file::Path mHeightMapName;
  float mWorldHeight;
  float mWorldSize;
  lev2::TerrainDrawableData _visualData;
};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectControllerData : public ComponentData {
  RttiDeclareConcrete(BulletObjectControllerData, ComponentData);

public:
  BulletObjectControllerData();
  ~BulletObjectControllerData();
  float GetRestitution() const { return mfRestitution; }
  float GetFriction() const { return mfFriction; }
  float GetMass() const { return mfMass; }
  float GetAngularDamping() const { return mfAngularDamping; }
  float GetLinearDamping() const { return mfLinearDamping; }
  bool GetAllowSleeping() const { return mbAllowSleeping; }
  bool GetKinematic() const { return mbKinematic; }

  const ork::ObjectMap GetForceControllerData() const { return mForceControllerDataMap; }
  const BulletShapeBaseData* GetShapeData() const;
  void SetShapeData(BulletShapeBaseData* pdata) { mShapeData = pdata; }

  void ShapeGetter(ork::rtti::ICastable*& val) const;
  void ShapeSetter(ork::rtti::ICastable* const& val);

protected:
  friend class BulletObjectControllerInst;

  ComponentInst* createComponent(Entity* pent) const final;
  void DoRegisterWithScene(ork::ent::SceneComposer& sc) final;

  const BulletShapeBaseData* mShapeData;
  ork::ObjectMap mForceControllerDataMap;
  float mfRestitution;
  float mfFriction;
  float mfMass;
  float mfAngularDamping;
  float mfLinearDamping;
  bool mbAllowSleeping;
  bool mbKinematic;
  bool _disablePhysics;
};

class BulletObjectControllerInst : public ork::ent::ComponentInst {
  RttiDeclareAbstract(BulletObjectControllerInst, ork::ent::ComponentInst);

public:
  BulletObjectControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* entity);
  ~BulletObjectControllerInst();

  btRigidBody* GetRigidBody() { return mRigidBody; }
  const BulletObjectControllerData& GetData() const { return mBOCD; }

  const BulletShapeBaseInst* GetShapeInst() const { return mShapeInst; }
  BulletObjectForceControllerInst* getForceController(PoolString ps) const;

private:
  const BulletObjectControllerData& mBOCD;
  btRigidBody* mRigidBody;
  orkmap<PoolString, BulletObjectForceControllerInst*> mForceControllerInstMap;
  BulletShapeBaseInst* mShapeInst;

  void DoUpdate(ork::ent::Simulation* inst) final;
  void doNotify(const ork::event::Event* event) final { return false; }
  bool DoLink(Simulation* psi) final;
  void DoStop(Simulation* psi) final;
};

///////////////////////////////////////////////////////////////////////////////

btBoxShape* XgmModelToBoxShape(const ork::lev2::XgmModel* xgmmodel, float fscale);
btSphereShape* XgmModelToSphereShape(const ork::lev2::XgmModel* xgmmodel, float fscale);
btCompoundShape* XgmModelToCompoundShape(const ork::lev2::XgmModel* xgmmodel, float fscale);
btCompoundShape* XgmMeshToCompoundShape(const ork::lev2::XgmMesh* xgmmesh, float fscale);
btCollisionShape* XgmClusterToBvhTriangleMeshShape(ork::lev2::xgmcluster_ptr_t xgmcluster, float fscale);
btCollisionShape* XgmModelToGimpactShape(const ork::lev2::XgmModel* xgmmodel, float fscale);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
