////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector3.h>
#include <ork/math/line.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct Sphere {
  fvec3 mCenter;
  float mRadius;

  Sphere(const fvec3& pos, float r) : mCenter(pos), mRadius(r) {}
  Sphere(const fvec3& boxmin, const fvec3& boxmax);

  void SupportMapping(const fvec3& v, fvec3& result) const;

  bool Intersect(const fray3& ray, fvec3& isect_in, fvec3& isect_out, fvec3& sphnormal) const;
};

///////////////////////////////////////////////////////////////////////////////

inline void Sphere::SupportMapping(const fvec3& v, fvec3& result) const {}

inline bool Sphere::Intersect(const fray3& ray, fvec3& isect_in, fvec3& isect_out, fvec3& isect_normal) const {
  fvec3 L = mCenter - ray.mOrigin;
  float tca = L.Dot(ray.mDirection);
  if (tca < 0.0f)
    return false;
  float d2 = L.Dot(L) - (tca * tca);
  float r2 = (mRadius * mRadius);
  if (d2 > r2)
    return false;
  float thc = CFloat::Sqrt(r2 - d2);
  isect_in = ray.mOrigin + ray.mDirection * (tca - thc);
  isect_out = ray.mOrigin + ray.mDirection * (tca + thc);
  isect_normal = (isect_in - mCenter).Normal();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct Circle {
  fvec2 mCenter;
  float mRadius;

  Circle(const fvec2& pos, float r) : mCenter(pos), mRadius(r) {}
};

//////////////////////////
// TRIANGLE INTERSECT

//  http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld019.htm
//  http://www.cs.princeton.edu/courses/archive/fall00/cs426/lectures/raycast/sld021.htm

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
