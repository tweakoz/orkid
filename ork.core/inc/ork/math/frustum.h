////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/line.h>
#include <ork/math/plane.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Frustum {
  typedef Ray3<float> ray_type;
  typedef fvec3 vec3_type;
  typedef fvec4 vec4_type;
  typedef Plane<float> plane_type;
  typedef fmtx4 mtx44_type;

  plane_type mNearPlane;
  plane_type mFarPlane;
  plane_type mLeftPlane;
  plane_type mRightPlane;
  plane_type mTopPlane;
  plane_type mBottomPlane;

  vec3_type mNearCorners[4];
  vec3_type mFarCorners[4];
  vec3_type mCenter;
  vec3_type mXNormal;
  vec3_type mYNormal;
  vec3_type mZNormal;

  void SupportMapping(const vec3_type& v, vec3_type& result) const;

  void Set(const mtx44_type& VMatrix, const mtx44_type& PMatrix);
  void Set(const mtx44_type& IVPMatrix);

  void CalcCorners();

  bool Contains(const vec3_type& v) const;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
