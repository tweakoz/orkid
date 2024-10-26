////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/ecs/physics/bullet.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <ork/ecs/scene.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/entity.inl>

#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>

#include "bullet_impl.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ecs {
///////////////////////////////////////////////////////////////////////////////

btVector3 orkv3tobtv3(const ork::fvec3& v3) {
  return btVector3(v3.x, v3.y, v3.z);
}
ork::fvec3 btv3toorkv3(const btVector3& v3) {
  return ork::fvec3(float(v3.x()), float(v3.y()), float(v3.z()));
}

ork::fquat btqtoorkq(const btQuaternion& q) {
  ork::fquat q_out;
  q_out.x = q.x();
  q_out.y = q.y();
  q_out.z = q.z();
  q_out.w = q.w();
  return q_out.inverse();
}

///////////////////////////////////////////////////////////////////////////////

btTransform orkmtx4tobtmtx4(const ork::fmtx4& mtx) {
  btTransform xf;
  ork::fvec3 position = mtx.translation();
  xf.setOrigin(orkv3tobtv3(position));
  btMatrix3x3& mtx33 = xf.getBasis();
  for (int i = 0; i < 3; i++) {
    float fx = mtx[0,i];
    float fy = mtx[1,i];
    float fz = mtx[2,i];
    mtx33[i] = btVector3(fx, fy, fz);
  }
  return xf;
}

ork::fmtx4 btmtx4toorkmtx4(const btTransform& mtx) {
  ork::fmtx4 rval;
  ork::fvec3 position = btv3toorkv3(mtx.getOrigin());
  rval.setTranslation(position);
  const btMatrix3x3& mtx33 = mtx.getBasis();
  for (int i = 0; i < 3; i++) {
    const btVector3& vec = mtx33.getColumn(i);
    rval[i, 0] = float(vec.x());
    rval[i, 1] = float(vec.y());
    rval[i, 2] = float(vec.z());
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

btMatrix3x3 orkmtx3tobtbasis(const ork::fmtx3& mtx) {
  btMatrix3x3 xf;
  for (int i = 0; i < 3; i++) {
    float fx = mtx.elemYX(i, 0);
    float fy = mtx.elemYX(i, 1);
    float fz = mtx.elemYX(i, 2);
    xf[i]    = btVector3(fx, fy, fz);
  }
  return xf;
}

ork::fmtx3 btbasistoorkmtx3(const btMatrix3x3& mtx) {
  ork::fmtx3 rval;
  for (int i = 0; i < 3; i++) {
    const btVector3& vec = mtx.getColumn(i);
    rval.setElemXY(i, 0, float(vec.x()));
    rval.setElemXY(i, 1, float(vec.y()));
    rval.setElemXY(i, 2, float(vec.z()));
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

EntMotionState::EntMotionState(const btTransform& initialpos, ork::ecs::Entity* entity) {
  mEntity    = entity;
  mTransform = initialpos;
}

void EntMotionState::getWorldTransform(btTransform& transform) const {
  transform = mTransform;
}

void EntMotionState::setWorldTransform(const btTransform& transform) {

  _counter++;
  
  if (mEntity) {

    ork::fvec3 position = btv3toorkv3(transform.getOrigin());
    ork::fquat rotation = btqtoorkq(transform.getRotation());

    if (_idata) {

      fmtx4 c;
      c.compose(position, rotation);
      _idata->_worldmatrices[_instance_id] = c;
      auto dpos = ork::fvec3_to_dvec3(position);
      auto delta = (dpos-_prevpos).absolute();
      _energy += delta;
      _energy = _energy * 0.99;
      _prevpos = dpos;
    } else {

      auto out_xform          = mEntity->transform();
      out_xform->_translation = position;
      out_xform->_rotation    = rotation;
      mTransform = transform;

    }
    // compute 4x4 matrix from btTransform
  }
}

}} // namespace ork::ecs
