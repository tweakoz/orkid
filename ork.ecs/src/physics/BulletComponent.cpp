////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>

#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>

#include <ork/math/basicfilters.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <ork/ecs/scene.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/scene.inl>
#include <ork/ecs/entity.inl>
#include <ork/ecs/simulation.inl>

#include <ork/ecs/physics/bullet.h>
#include "bullet_impl.h"
#include "../core/message_private.h"
#include "../scripting/LuaBindings.h"

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::BulletObjectComponentData, "EcsBulletObjectComponentData");
ImplementReflectionX(ork::ecs::BulletObjectComponent, "EcsBulletObjectComponent");
ImplementReflectionX(ork::ecs::BulletShapeBaseData, "EcsBulletShapeBaseData");
ImplementReflectionX(ork::ecs::BulletShapeCapsuleData, "EcsBulletShapeCapsuleData");
ImplementReflectionX(ork::ecs::BulletShapePlaneData, "EcsBulletShapePlaneData");
ImplementReflectionX(ork::ecs::BulletShapeSphereData, "EcsBulletShapeSphereData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

BulletObjectComponentData::BulletObjectComponentData() {
}
BulletObjectComponentData::~BulletObjectComponentData() {
  //	if( _collisionShape )
  //		delete _collisionShape;
}

void BulletObjectComponentData::DoRegisterWithScene(SceneComposer& sc) const {
  sc.Register<BulletSystemData>();
}

void BulletObjectComponentData::describeX(ComponentDataClass* clazz) {
  //	reflect::RegisterProperty( "Model", & BulletObjectComponentData::GetModelAccessor, &
  // BulletObjectComponentData::SetModelAccessor );
  clazz->floatProperty("Restitution", float_range{0, 5}, &BulletObjectComponentData::_restitution);
  clazz->floatProperty("Friction", float_range{0, 1}, &BulletObjectComponentData::_friction);
  clazz->floatProperty("Mass", float_range{0, 1000}, &BulletObjectComponentData::_mass);
  clazz->floatProperty("LinearDamping", float_range{0, 1}, &BulletObjectComponentData::_linearDamping);
  clazz->floatProperty("AngularDamping", float_range{0, 1}, &BulletObjectComponentData::_angularDamping);

  clazz->directProperty("AllowSleeping", &BulletObjectComponentData::_allowSleeping);
  clazz->directProperty("IsKinematic", &BulletObjectComponentData::_isKinematic);
  clazz->directProperty("Disable", &BulletObjectComponentData::_disablePhysics);

  clazz->directObjectMapProperty("ForceControllers", &BulletObjectComponentData::_forcedatas);

  // reflect::annotatePropertyForEditor<BulletObjectComponentData>(
  //  "ForceControllers", "editor.factorylistbase", "BulletObjectForceControllerData");
  // reflect::annotatePropertyForEditor<BulletObjectComponentData>("ForceControllers", "editor.map.policy.impexp", "true");
  // reflect::RegisterProperty("Shape", &BulletObjectComponentData::ShapeGetter, &BulletObjectComponentData::ShapeSetter);
  // reflect::annotatePropertyForEditor<BulletObjectComponentData>("Shape", "editor.factorylistbase", "BulletShapeBaseData");
}
///////////////////////////////////////////////////////////////////////////////
void BulletObjectComponentData::ShapeGetter(ork::rtti::ICastable*& val) const {
  // BulletShapeBaseData* nonconst = const_cast<BulletShapeBaseData*>(_shapedata);
  // val                           = nonconst;
  OrkAssert(false);
}
///////////////////////////////////////////////////////////////////////////////
void BulletObjectComponentData::ShapeSetter(ork::rtti::ICastable* const& val) {
  // ork::rtti::ICastable* ptr = val;
  //_shapedata                = ((ptr == 0) ? 0 : rtti::safe_downcast<BulletShapeBaseData*>(ptr));
  OrkAssert(false);
}
///////////////////////////////////////////////////////////////////////////////
Component* BulletObjectComponentData::createComponent(Entity* pent) const {
  BulletObjectComponent* pinst = new BulletObjectComponent(*this, pent);
  return pinst;
}

object::ObjectClass* BulletObjectComponentData::componentClass() {
  return BulletObjectComponent::GetClassStatic();
}

/////////////////////////////////////////////////////////////////////////////////////

void BulletObjectComponent::describeX(object::ObjectClass* clazz) {
}
BulletObjectComponent::BulletObjectComponent(const BulletObjectComponentData& data, Entity* pent)
    : Component(&data, pent)
    , mBOCD(data) {

  if (mBOCD._disablePhysics)
    return;

  pent->simulation()->findSystem<BulletSystem>();

  for (auto item : data._forcedatas ) {
    const auto& forcename                = item.first;
    auto forcedata = item.second;
    if (forcedata) {
      auto force = forcedata->CreateForceControllerInst(data, pent);
      _forces[forcename]     = force;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////

BulletObjectComponent::~BulletObjectComponent() {
  if (_shapeinst)
    delete _shapeinst;
}

///////////////////////////////////////////////////////////////////////////////////////

bool BulletObjectComponent::_onLink(Simulation* sim) {

  //sim->debugBanner(255, 128, 0, "BulletObjectComponent<%p> _onLink\n", (void*)this);
  if (mBOCD._disablePhysics)
    return true;

  auto bulletsys = sim->findSystem<BulletSystem>();
  bulletsys->_onLinkComponent(this);
  return true;
}

void BulletObjectComponent::_onUnlink(Simulation* sim) {
  //sim->debugBanner(255, 128, 0, "BulletObjectComponent<%p> _onUnlink\n", (void*)this);
  auto bulletsys = sim->findSystem<BulletSystem>();
  bulletsys->_onUnlinkComponent(this);
}

///////////////////////////////////////////////////////////////////////////////////////

bool BulletObjectComponent::_onStage(Simulation* sim) {
  auto bulletsys = sim->findSystem<BulletSystem>();
  bulletsys->_onStageComponent(this);
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletObjectComponent::_onUnstage(Simulation* sim) {
  //sim->debugBanner(255, 128, 0, "BulletObjectComponent<%p> _onUnstage\n", (void*)this);
  auto bulletsys = sim->findSystem<BulletSystem>();
  bulletsys->_onUnstageComponent(this);
}

///////////////////////////////////////////////////////////////////////////////////////

bool BulletObjectComponent::_onActivate(Simulation* sim) {
  //sim->debugBanner(255, 128, 0, "BulletObjectComponent<%p> _onActivate\n", (void*)this);
  auto bulletsys = sim->findSystem<BulletSystem>();
  bulletsys->_onActivateComponent(this);
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletObjectComponent::_onDeactivate(Simulation* sim) {

  //sim->debugBanner(255, 128, 0, "BulletObjectComponent<%p> _onDeactivate\n", (void*)this);

  if (mBOCD._disablePhysics)
    return;

  auto bulletsys = sim->findSystem<BulletSystem>();
  bulletsys->_onDeactivateComponent(this);
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletObjectComponent::update(Simulation* sim, float time_step) {

  if (mBOCD._disablePhysics)
    return;

  if (_rigidbody) {
    //////////////////////////////////////////////////
    // copy motion state to entity transform
    //////////////////////////////////////////////////

    auto ecs_xform = GetEntity()->transform();

    if (mBOCD._isKinematic) {
      btMotionState* motionState = _rigidbody->getMotionState();
      auto ork_mtx               = ecs_xform->composed();
      btTransform xf             = orkmtx4tobtmtx4(ork_mtx);
      motionState->setWorldTransform(xf);
      //sim->debugBanner(255, 128, 0, "BulletObjectComponent<%p> iskin\n", (void*)this);
    } else {
      const btMotionState* motionState = _rigidbody->getMotionState();
      btTransform xf;
      motionState->getWorldTransform(xf);

      ork::fvec3 position = btv3toorkv3(xf.getOrigin());
      ork::fquat rotation = btqtoorkq(xf.getRotation());

      ecs_xform->_translation = position;
      ecs_xform->_rotation = rotation;

      //auto mtx    = ecs_xform->composed();
      //auto mtxstr = mtx.dump4x3cn();
      //printf(" bulletcomp::update value<%s>\n", mtxstr.c_str());

      //sim->debugBanner(
          //255, 128, 0, "BulletObjectComponent<%p> newpos<%f %f %f>\n", (void*)this, position.x, position.y, position.z);
    }

    //////////////////////////////////////////////////
    // update dynamic rigid body params
    //////////////////////////////////////////////////

    _rigidbody->setRestitution(mBOCD._restitution);
    _rigidbody->setFriction(mBOCD._friction);
    _rigidbody->setDamping(mBOCD._linearDamping, mBOCD._angularDamping);

    //////////////////////////////////////////////////
    // apply forces
    //////////////////////////////////////////////////

    for (auto item : _forces) {
      auto forcecontroller = item.second;
      if (forcecontroller)
        forcecontroller->UpdateForces(this,time_step);
    }

    // apply BulletSystem's expgravity
  
    //////////////////////////////////////////////////
    // printf( "newpos<%f %f %f>\n", pos.GetX(), pos.GetY(), pos.GetZ() );
  }
}

///////////////////////////////////////////////////////////////////////////////////////

BulletObjectForceControllerInst* BulletObjectComponent::getForceController(std::string named) const {

  BulletObjectForceControllerInst* rval = nullptr;
  auto it                               = _forces.find(named);
  if (it != _forces.end()) {
    return it->second;
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletObjectComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data ) {
  switch (evID.hashed()) {
    case "SetDirectionalForce"_crcu: {
      const auto& vars = data.get<ScriptTable>()._items;
      auto it_fc = vars.find("ForceController");
      auto fc_response = it_fc->second._encoded.get<impl::comp_response_ptr_t>();
      auto fc = fc_response->_responseData.get<BulletObjectForceControllerInst*>();
      auto dirfc = dynamic_cast<DirectionalForceInst*>(fc);

      auto it_force = vars.find("Force");
      dirfc->_force = it_force->second._encoded.get<double>();

      auto it_dir = vars.find("Direction");
      dirfc->_direction = it_dir->second._encoded.get<fvec3>();

      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}
void BulletObjectComponent::_onRequest(Simulation* psi, impl::comp_response_ptr_t response, token_t evID, evdata_t data) {
  switch (evID.hashed()) {
    case "CreateDirectionalForce"_crcu: {

      const auto& vars = data.get<ScriptTable>()._items;
      auto it_name = vars.find("name");
      auto name = it_name->second._encoded.get<std::string>();
      //printf( "GOT NAME<%s>\n", name.c_str() );

      auto new_force = new DirectionalForceInst();


      _forces[name] = new_force;

      response->_responseData.set<BulletObjectForceControllerInst*>(new_force);
      break;
    }
    default:
      OrkAssert(false);
      break;
  }  
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeBaseData::describeX(object::ObjectClass* clazz) {
}
BulletShapeBaseData::BulletShapeBaseData()
    : _shapeFactory(nullptr) {
}
BulletShapeBaseData::~BulletShapeBaseData() {
}

BulletShapeBaseInst* BulletShapeBaseData::CreateShape(const ShapeCreateData& data) const {
  BulletShapeBaseInst* rval = nullptr;

  if (_shapeFactory._createShape) {
    rval = _shapeFactory._createShape(data);

    if (rval && rval->_collisionShape) {
      btTransform ident;
      ident.setIdentity();

      btVector3 bbmin;
      btVector3 bbmax;

      rval->_collisionShape->getAabb(ident, bbmin, bbmax);
      rval->mBoundingBox.SetMinMax(btv3toorkv3(bbmin), btv3toorkv3(bbmax));
    }
  }

  OrkAssert(rval);

  return rval;
}

void BulletShapeBaseData::doNotify(const event::Event* event) {
  // if (auto pgev = dynamic_cast<const ObjectGedEditEvent*>(event)) {
  // opq::updateSerialQueue()->enqueue([this]() { _shapeFactory._invalidate(this); });
  //}
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeCapsuleData::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("Extent", float_range{0.1, 1000.0}, &BulletShapeCapsuleData::mfExtent);
  clazz->floatProperty("Radius", float_range{0.1, 1000.0}, &BulletShapeCapsuleData::mfRadius);
}

BulletShapeCapsuleData::BulletShapeCapsuleData()
    : mfRadius(0.10f)
    , mfExtent(1.0f) {
  _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval             = new BulletShapeBaseInst(this);
    rval->_collisionShape = new btCapsuleShapeZ(this->mfRadius, this->mfExtent);
    return rval;
  };
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapePlaneData::describeX(object::ObjectClass* clazz) {
}

BulletShapePlaneData::BulletShapePlaneData() {
  _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);
    auto up   = fvec3(0, 1, 0);

    rval->_collisionShape = new btStaticPlaneShape(orkv3tobtv3(up), 0.0f);
    return rval;
  };
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeSphereData::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("Radius", float_range{0.1, 1000.0}, &BulletShapeSphereData::_radius);
}

BulletShapeSphereData::BulletShapeSphereData() {
  _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval             = new BulletShapeBaseInst(this);
    rval->_collisionShape = new btSphereShape(this->_radius);
    return rval;
  };
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeBaseInst::BulletShapeBaseInst(const BulletShapeBaseData* data) {
  _shapeData = data;
  _impl      = nullptr;
}

} // namespace ork::ecs
