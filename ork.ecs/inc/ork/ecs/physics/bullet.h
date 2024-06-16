////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/math/PIDController.inl>

#include <ork/math/box.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>

#include <ork/lev2/gfx/terrain/terrain_drawable.h>

#include <ork/ecs/component.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/system.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

struct BulletShapeBaseInst;
struct BulletObjectComponentData;
struct PhysicsDebugger;
struct BulletObjectComponent;
struct BulletSystem;
struct BulletObjectForceControllerInst;
struct BulletShapeBaseData;
struct BulletObjectForceControllerData;
struct DirectionalForceData;
struct BulletShapePlaneData;
struct BulletShapeMeshData;
struct BulletShapeSphereData;
struct BulletShapeCapsuleData;
struct ShapeFactory;

using shapedata_ptr_t = std::shared_ptr<BulletShapeBaseData>;
using shapedata_constptr_t = std::shared_ptr<const BulletShapeBaseData>;
using forcecontrollerdata_ptr_t = std::shared_ptr<BulletObjectForceControllerData>;
using forcemap_t = std::map<std::string,forcecontrollerdata_ptr_t>;
using directionalfcdata_ptr_t = std::shared_ptr<DirectionalForceData>;
using shape_factory_t = ShapeFactory;
using bulletshapeplanedata_ptr_t = std::shared_ptr<BulletShapePlaneData>;
using bulletshapespheredata_ptr_t = std::shared_ptr<BulletShapeSphereData>;
using bulletshapecapsuledata_ptr_t = std::shared_ptr<BulletShapeCapsuleData>;
using bulletshapemeshdata_ptr_t = std::shared_ptr<BulletShapeMeshData>;
using bulletobjectcomponentdata_ptr_t = std::shared_ptr<BulletObjectComponentData>;
///////////////////////////////////////////////////////////////////////////////

struct BulletObjectComponentData : public ComponentData {
  DeclareConcreteX(BulletObjectComponentData, ComponentData);

public:


  BulletObjectComponentData();
  ~BulletObjectComponentData();

  void ShapeGetter(ork::rtti::ICastable*& val) const;
  void ShapeSetter(ork::rtti::ICastable* const& val);

  Component* createComponent(Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(SceneComposer& sc) const final;

  forcemap_t _forcedatas;

  shapedata_ptr_t _shapedata = nullptr;
  float _restitution = 0.5f;
  float _friction = 0.5f;
  float _mass = 1.0f;
  float _angularDamping = 0.5f;
  float _linearDamping = 0.5f;
  bool _allowSleeping = true;
  bool _isKinematic = false;
  bool _disablePhysics = false;
  uint32_t _groupAssign = 1;
  uint32_t _groupCollidesWith = 0xffffffff;
  script_cb_t _collisionCallback;
  fvec3 _angularFactor;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletSystemData : public SystemData {
  DeclareConcreteX(BulletSystemData, SystemData);

  float mfTimeScale = 1.0f;
  float mSimulationRate = 240.0f;
  bool _debug = false;
  fvec3 _lingravity;
  fvec3 _expgravity;

public:
  BulletSystemData();

  float GetTimeScale() const { return mfTimeScale; }
  bool IsDebug() const { return _debug; }
  float GetSimulationRate() const { return mSimulationRate; }
  const fvec3& GetGravity() const { return _lingravity; }

protected:
  System* createSystem(Simulation* psi) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletObjectForceControllerData : public ork::Object {
  DeclareAbstractX(BulletObjectForceControllerData, ork::Object);

public:
  BulletObjectForceControllerData();
  ~BulletObjectForceControllerData();

  virtual BulletObjectForceControllerInst* CreateForceControllerInst(const BulletObjectComponentData& data,
                                                                     Entity* pent) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct DirectionalForceData final : public BulletObjectForceControllerData {
  DeclareConcreteX(DirectionalForceData, BulletObjectForceControllerData);

public:
  DirectionalForceData()
      : _force(1.0f)
      , _direction(0.0f, 0.0f, 0.0f) {
  }

  ~DirectionalForceData() {
  }

  BulletObjectForceControllerInst*
  CreateForceControllerInst(const BulletObjectComponentData& data, Entity* pent) const final;

  float _force;
  fvec3 _direction;
};
///////////////////////////////////////////////////////////////////////////////

struct ShapeCreateData {
  Entity* mEntity;
  BulletSystem* mWorld;
  BulletObjectComponent* mObject;
};

///////////////////////////////////////////////////////////////////////////////

struct ShapeFactory {

  typedef std::function<BulletShapeBaseInst*(const ShapeCreateData& data)> creator_t;
  typedef std::function<void(BulletShapeBaseData*)> invalidator_t;

  ShapeFactory(creator_t c = nullptr, invalidator_t i = [](BulletShapeBaseData*) {})
      : _createShape(c), _invalidate(i), _impl(nullptr) {}

  creator_t _createShape;
  invalidator_t _invalidate;
  svarp_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletShapeBaseData : public ork::Object {
  DeclareAbstractX(BulletShapeBaseData, ork::Object);

public:
  BulletShapeBaseData();
  ~BulletShapeBaseData();

  BulletShapeBaseInst* CreateShape(const ShapeCreateData& data) const;

protected:
  shape_factory_t _shapeFactory;

  void doNotify(const event::Event* event) override;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletShapeCapsuleData : public BulletShapeBaseData {
  DeclareConcreteX(BulletShapeCapsuleData, BulletShapeBaseData);

public:
  BulletShapeCapsuleData();

  float mfRadius;
  float mfExtent;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletShapePlaneData : public BulletShapeBaseData {
  DeclareConcreteX(BulletShapePlaneData, BulletShapeBaseData);

public:
  BulletShapePlaneData();
  fvec3 _pos;
  fvec3 _nrm;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletShapeSphereData : public BulletShapeBaseData {
  DeclareConcreteX(BulletShapeSphereData, BulletShapeBaseData);

public:
  BulletShapeSphereData();

  float _radius = 1.0f;
};
///////////////////////////////////////////////////////////////////////////////

struct BulletShapeMeshData : public BulletShapeBaseData {
  DeclareConcreteX(BulletShapeMeshData, BulletShapeBaseData);

public:
  BulletShapeMeshData();
  ~BulletShapeMeshData();

  //lev2::XgmModelAsset* asset() { return mModelAsset; }
  //void SetModelAccessor(ork::rtti::ICastable* const& mdl);
  //void GetModelAccessor(ork::rtti::ICastable*& mdl) const;

  AssetPath _meshpath;
  fvec3 _scale;
  fvec3 _translation;

  mutable meshutil::flatsubmesh_ptr_t _flatmesh;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletShapeTerrainData : public BulletShapeBaseData {
  DeclareConcreteX(BulletShapeTerrainData, BulletShapeBaseData);

public:
  BulletShapeTerrainData();
  ~BulletShapeTerrainData();

  //void -(file::Path const& lmap);
  //void GetHeightMapName(file::Path& lmap) const;

  //const file::Path& HeightMapPath() const { return mHeightMapName; }
  //float WorldHeight() const { return mWorldHeight; }
  //float WorldSize() const { return mWorldSize; }

  file::Path _heightMapPath;
  float _worldHeight = 1000.0f;
  float _worldSize = 1000.0f;
  lev2::TerrainDrawableData _visualData;

private:
  ork::Object* _visualDataAccessor() { return & _visualData; }
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) final;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
