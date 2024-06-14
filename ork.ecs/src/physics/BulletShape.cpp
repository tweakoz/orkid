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

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////////////
// BASE
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

///////////////////////////////////////////////////////////////////////////////

BulletShapeBaseInst::BulletShapeBaseInst(const BulletShapeBaseData* data) {
  _shapeData = data;
  _impl      = nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////
// CAPSULE
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
// PLANE
///////////////////////////////////////////////////////////////////////////////////////

void BulletShapePlaneData::describeX(object::ObjectClass* clazz) {
}

BulletShapePlaneData::BulletShapePlaneData() {

  _pos = fvec3(0, 0, 0);
  _nrm = fvec3(0, 1, 0);

  _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);
    auto up   = _nrm.normalized();
    float distance = _pos.dotWith(up);

    rval->_collisionShape = new btStaticPlaneShape(orkv3tobtv3(up), distance);
    return rval;
  };
}

///////////////////////////////////////////////////////////////////////////////////////
// SPHERE
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

} // namespace ork::ecs

///////////////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::BulletShapeBaseData, "EcsBulletShapeBaseData");
ImplementReflectionX(ork::ecs::BulletShapeCapsuleData, "EcsBulletShapeCapsuleData");
ImplementReflectionX(ork::ecs::BulletShapePlaneData, "EcsBulletShapePlaneData");
ImplementReflectionX(ork::ecs::BulletShapeSphereData, "EcsBulletShapeSphereData");
