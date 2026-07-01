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
  float _sleepThresholdLinear = 0.8f;
  float _sleepThresholdAngular = 1.0f;
  bool _allowSleeping = true;
  bool _isKinematic = false;
  bool _disablePhysics = false;
  uint32_t _groupAssign = 1;
  uint32_t _groupCollidesWith = 0xffffffff;
  script_cb_t _collisionCallback;
  // E.2-walk: forward this body's contacts to the scene's PythonSystem as "Collision"
  // notifies ({nameA,nameB,point,normal}) — the input/system SCRIPT intercepts them.
  bool _notifyCollisions = false;
  fvec3 _angularFactor;
  bool _syncShapeScale = false;
  std::string _instanceNodeName;
  lev2::scenegraph::node_instance_data_ptr_t _INSTANCEDATA;
};

///////////////////////////////////////////////////////////////////////////////

struct BulletSystemData : public SystemData {
  DeclareConcreteX(BulletSystemData, SystemData);

  float mfTimeScale = 1.0f;
  float mSimulationRate = 240.0f;
  bool _debug = false;
  fvec3 _lingravity;
  fvec3 _expgravity;
  bool _test_deactivation = false;

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
  mutable meshutil::submesh_ptr_t _submesh;
};

///////////////////////////////////////////////////////////////////////////////

// BulletShapeScatterData (E.2-walk follow-on) — ONE static compound collider for a placed
// ScatterSet: each item contributes a primitive proxy CHILD whose kind+dims come from the
// SET ITSELF (the placer's per-point proxy_kind/proxy_dims channels, declared at the
// scatter() sink — NOTHING hardcoded here). Child transforms come from the baked per-item
// xform (uniform scale folded into the proxy dims; bullet children take rigid transforms).
// Resolves the .ogeo like ScatterSource: `_ogeo_path` direct, or the PORTABLE
// <assetcache>/terrain/<scatter_asset>/<sink>.ogeo convention. Broadphase = ONE body;
// bullet's compound carries an internal AABB tree, so thousands of children stay cheap.
struct BulletShapeScatterData : public BulletShapeBaseData {
  DeclareConcreteX(BulletShapeScatterData, BulletShapeBaseData);

public:
  BulletShapeScatterData();
  std::string _scatter_asset; // HeightField asset name (the portable reference)
  std::string _sink;          // scatter sink name on that asset
  std::string _ogeo_path;     // direct path override (tools; takes precedence)
};
using bulletshapescatterdata_ptr_t = std::shared_ptr<BulletShapeScatterData>;

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
  // E.2-walk: the ASSET-WIRED form (preferred). Non-empty = resolve the baked HeightField
  // artifact <assetcache>/terrain/<hf_asset>/height.exr and take worldSize/worldHeight
  // from its .terrain.json manifest (extent_m / height_m) — physics collides with EXACTLY
  // what the chunk renderer draws, from ONE scene declaration. _heightMapPath/_worldSize/
  // _worldHeight above remain the direct-path override (tools / legacy scenes).
  // NOTE bullet centers the heightfield AABB: the owning ENTITY must sit at
  // y = (minH+maxH)/2 * worldHeight; the baked height channel is auto-exposed to [0,1]
  // exactly, so that is 0.5 * worldHeight (the scene walker/terrain_collider helper does this).
  std::string _hf_asset;
  // 0 => collide at the EXR's full (bake) resolution. >0 => the heightmap is loaded as a lev2::Image and
  // high-quality-resampled (ringing-free, Image::resampledOf) DOWN to this grid, so physics collides with
  // exactly the downsampled surface the render mesh draws (terrain render_dimension). Set by terrain().
  int _render_dimension = 0;
  lev2::TerrainDrawableData _visualData;

private:
  ork::Object* _visualDataAccessor() { return & _visualData; }
  bool postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) final;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
